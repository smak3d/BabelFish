// Тест серверного конвейера (п.4): Server целиком — состояния, диспетчер
// ServerAPI, настоящие NetworkServer/NetworkClient поверх zmq/TCP на
// loopback. Модели НЕ грузятся: быстрые фазы проверяют приём, сборку,
// диспетчеризацию и ответ, а полный проход через Transcriber и
// LocalAssistant запускается вручную:
//
//     PipelineFlow --with-models
//
// иначе каждый ctest ждал бы загрузки моделей в полтора и три гигабайта.
//
// Порт подбирается свободный и слушается как tcp://127.0.0.1:<port>:
// общедоступный bind (tcp://*:port) тестом не покрыт нарочно — дёргал бы
// брандмауэр (см. todo п.4, та же причина, по которой TransportEcho
// работает только с 127.0.0.1).

#include "Server.h"

#include "NetworkClient.h"

#include "NetworkError.h"

#include "MessageType.h"
#include "Request.h"
#include "Response.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

namespace
{
    int checksRun    = 0;
    int checksFailed = 0;

    void check(bool condition, const char* expression, int line)
    {
        ++checksRun;
        if (!condition)
        {
            std::printf("FAIL line %d: %s\n", line, expression);
            ++checksFailed;
        }
    }

    // Прогресс печатаем сразу: если фаза всё же зависнет, по логу ctest
    // будет видно, на которой именно.
    void step(const char* what)
    {
        std::printf("- %s\n", what);
        std::fflush(stdout);
    }

    // Прогоняем конвейер до возврата в IdleState: каждый update()
    // продвигает ровно на одно состояние, а Idle — точка, в которой сервер
    // снова ждёт следующий кадр. Шаги ограничены: упавший конвейер должен
    // завалить проверку, а не держать ctest бесконечным циклом.
    bool pump(Server& server)
    {
        for (int stepNumber = 0; stepNumber < 8; ++stepNumber)
        {
            server.update();
            if (server.isIdle())
                return true;
        }

        return false;
    }

    // Отправить запрос, прогнать конвейер, забрать ответ. nullopt =
    // сбой на любом из трёх шагов, причина уже напечатана.
    std::optional<protocol::Response> roundTrip(NetworkClient& client,
                                                Server& server,
                                                const protocol::Request& request)
    {
        if (!client.send(request.toMessage()))
        {
            std::printf("  client send failed: %s - %s\n",
                        networkErrorName(client.lastError()),
                        client.lastErrorText().c_str());
            return std::nullopt;
        }

        if (!pump(server))
        {
            std::printf("  conveyor did not return to idle\n");
            return std::nullopt;
        }

        const auto message = client.receive();
        if (!message)
        {
            std::printf("  client receive failed: %s - %s\n",
                        networkErrorName(client.lastError()),
                        client.lastErrorText().c_str());
            return std::nullopt;
        }

        return protocol::Response::fromMessage(*message);
    }

    protocol::Request makeAudioRequest(std::uint32_t id,
                                       const protocol::AudioFormat& format,
                                       const std::vector<std::int16_t>& samples)
    {
        protocol::Request request;
        request.id = id;
        request.setAudio(format, samples);
        return request;
    }
}

#define CHECK(expression) check((expression), #expression, __LINE__)

// ---------------------------------------------------------------------------
// Диспетчер: запросы, которые сервер не берёт в работу. Отказ уходит
// обычным SendingState — REP обязан ответить на каждый приём.
// ---------------------------------------------------------------------------
static void testRejects(NetworkClient& client, Server& server)
{
    step("диспетчер: отказы уходят ответом, а не зависшим REP");

    // Пустое аудио: данных нет — до Transcriber не доходит.
    auto response = roundTrip(client, server,
                              makeAudioRequest(1, { 16000, 1 }, {}));
    CHECK(response.has_value());
    CHECK(response && response->id == 1);
    CHECK(response && response->status == protocol::ResponseStatus::UnknownError);
    CHECK(response && response->response.find("empty") != std::string::npos);

    // Чужая частота: payload непустой, формат не наш — формат едет с
    // запросом (п.1), расхождение становится ошибкой, а не тишиной.
    response = roundTrip(client, server,
                         makeAudioRequest(2, { 48000, 1 }, { 100, 200, 300, 400 }));
    CHECK(response && response->id == 2);
    CHECK(response && response->status == protocol::ResponseStatus::TranscriptionError);
    CHECK(response && response->response.find("48000") != std::string::npos);

    // Неподдерживаемый тип запроса.
    protocol::Request raw;
    raw.id      = 3;
    raw.type    = protocol::MessageType::RawRequest;
    raw.payload = { std::byte{ 1 }, std::byte{ 2 }, std::byte{ 3 } };

    response = roundTrip(client, server, raw);
    CHECK(response && response->id == 3);
    CHECK(response && response->status == protocol::ResponseStatus::UnknownError);
    CHECK(response && response->response.find("unsupported") != std::string::npos);

    if (response)
        std::printf("    причина: %s\n", response->response.c_str());
}

// ---------------------------------------------------------------------------
// Битый кадр: REP обязан ответить на каждый приём, иначе следующий
// receive() упадёт с EFSM и сокет закроется.
// ---------------------------------------------------------------------------
static void testBrokenFrame(NetworkClient& client, Server& server)
{
    step("битый кадр: вежливый ответ вместо сломанного REP-цикла");

    // Заголовок читается (id вытащим для ответа), а тело кадра типу не
    // соответствует — Request::fromMessage откажет.
    const protocol::MessageHeader header{ protocol::MessageType::Result, 777 };
    auto broken = header.serialize();
    broken.push_back(std::byte{ 0x00 });

    CHECK(client.send(zmq::message_t(broken.data(), broken.size())));
    CHECK(pump(server)); // ответ ушёл прямо из Idle, сервер в строю

    const auto message = client.receive();
    CHECK(message.has_value());

    std::optional<protocol::Response> response;
    if (message)
        response = protocol::Response::fromMessage(*message);

    CHECK(response.has_value());
    CHECK(response && response->id == 777);
    CHECK(response && response->status == protocol::ResponseStatus::UnknownError);
}

// ---------------------------------------------------------------------------
// Обрыв приёма: ErrorState поднимает listen() заново на том же порту.
// ---------------------------------------------------------------------------
static void testRecovery(NetworkClient& client, Server& server)
{
    step("обрыв приёма: ErrorState переслушивает тот же порт сам");

    // Короткий таймаут и никто не шлёт: receive упадёт, сокет закроется
    // (контракт NetworkServer), конвейер уйдёт в ErrorState — и должен
    // вернуться в строй без вмешательства.
    server.getNetwork().setReceiveTimeout(50);
    CHECK(pump(server));
    CHECK(server.isIdle());
    server.getNetwork().setReceiveTimeout(10'000);

    // И следующий запрос после восстановления обслуживается как обычно.
    const auto response = roundTrip(client, server,
                                    makeAudioRequest(5, { 16000, 1 }, {}));
    CHECK(response && response->id == 5);
    CHECK(response && response->status == protocol::ResponseStatus::UnknownError);
}

// ---------------------------------------------------------------------------
// Дальше — только с --with-models: тут в работу по-настоящему входят
// Transcriber и LocalAssistant.
// ---------------------------------------------------------------------------

// Полный проход: текст (п.3 умеет TextRequest) сразу идёт ассистенту.
static void testTextProcessing(NetworkClient& client, Server& server)
{
    step("обработка: TextRequest -> LocalAssistant -> ответ клиенту");

    const std::string question = "Чем отличается REQ от REP?";

    protocol::Request request;
    request.id   = 6;
    request.type = protocol::MessageType::TextRequest;
    request.payload.resize(question.size());
    std::memcpy(request.payload.data(), question.data(), question.size());

    const auto response = roundTrip(client, server, request);

    CHECK(response.has_value());
    CHECK(response && response->id == 6);
    CHECK(response && response->status == protocol::ResponseStatus::Ok);
    // Текстовый запрос: transcription — это присланный текст, см.
    // ProcessingState.
    CHECK(response && response->transcription == question);
    CHECK(response && !response->response.empty());

    if (response)
        std::printf("    ответ ассистента: %s\n", response->response.c_str());
}

// Секунда тишины: whisper ничего не распознал — ассистента не спрашиваем,
// статус остаётся Ok (ветка пустой транскрипции в ProcessingState).
static void testSilenceProcessing(NetworkClient& client, Server& server)
{
    step("обработка: тишина -> Transcriber -> статус Ok");

    const std::vector<std::int16_t> silence(16000, 0);
    const auto response = roundTrip(client, server,
                                    makeAudioRequest(7, { 16000, 1 }, silence));

    CHECK(response.has_value());
    CHECK(response && response->id == 7);
    CHECK(response && response->status == protocol::ResponseStatus::Ok);

    if (response)
        std::printf("    transcription: \"%s\"\n    response: \"%s\"\n",
                    response->transcription.c_str(),
                    response->response.c_str());
}

int main(int argc, char* argv[])
{
    const bool withModels =
        argc > 1 && std::string(argv[1]) == "--with-models";

    // Свободный порт из ряда 47051..47070 (47001..47040 держит
    // TransportEcho). Слушаем 127.0.0.1, не tcp://*:port.
    std::optional<Server> server;
    std::uint16_t port = 0;

    for (std::uint16_t candidate = 47051; candidate < 47071; ++candidate)
    {
        server.emplace("tcp://127.0.0.1:" + std::to_string(candidate));

        if (server->isListening())
        {
            port = candidate;
            break;
        }
    }

    if (port == 0)
    {
        std::printf("pipeline flow: no free port in 47051..47070\n");
        return 1;
    }

    // Таймауты поменьше, чем в проде: зависшая фаза должна упасть за
    // секунды, а не держать ctest до своего лимита. receive подбирается
    // поменьше ещё и потому, что фазы сами шлют запрос перед pump():
    // приём обязан вернуться почти сразу.
    server->getNetwork().setReceiveTimeout(10'000);
    server->getNetwork().setSendTimeout(5'000);

    NetworkClient client;
    client.setReceiveTimeout(5'000);
    client.setSendTimeout(5'000);

    CHECK(client.connect("127.0.0.1", port));

    testRejects(client, *server);
    testBrokenFrame(client, *server);
    testRecovery(client, *server);

    if (withModels)
    {
        testTextProcessing(client, *server);
        testSilenceProcessing(client, *server);
    }
    else
    {
        std::printf(
            "- режим без моделей: обработка (Transcriber/LocalAssistant)\n"
            "  проверяется вручную: PipelineFlow --with-models\n");
    }

    std::printf("pipeline flow: %d checks, %d failed\n",
                checksRun, checksFailed);
    return checksFailed == 0 ? 0 : 1;
}
