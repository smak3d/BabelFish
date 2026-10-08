// Эхо-тест транспорта (шаг 2 плана): клиент шлёт тестовый Request, сервер
// возвращает его же.
//
// Настоящие zmq-сокеты и настоящий TCP, но loopback и один процесс — чтобы
// проверка запускалась из ctest, а не руками на двух машинах.
//
// Две хитрости, без которых тест получается занудным:
//
//   - каждый listen() идёт на новый порт: на Windows libzmq ставит на
//     слушающий сокет SO_EXCLUSIVEADDRUSE, поэтому перезабиндить порт, пока
//     на нём висят TIME_WAIT-соединения, не выходит — фазы бы падали по
//     минуте на каждую;
//   - фазы выстроены так, что сервер никогда не отвечает ушедшему клиенту:
//     REP обязан ответить на принятый запрос, поэтому «сервер молчит» — это
//     всегда последняя операция сервера в своей фазе.

#include "NetworkClient.h"
#include "NetworkServer.h"

#include "NetworkError.h"

#include "AudioFormat.h"
#include "MessageType.h"
#include "Request.h"

#include <cstdint>
#include <cstdio>
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

    void report(const char* owner, NetworkError error, const std::string& text)
    {
        std::printf("  %s: %s%s%s\n", owner, networkErrorName(error),
                    text.empty() ? "" : " - ", text.c_str());
        std::fflush(stdout);
    }

    protocol::Request makeRequest(std::uint32_t id)
    {
        protocol::Request request;
        request.id = id;

        std::vector<std::int16_t> samples;
        samples.reserve(2048);
        for (int i = 0; i < 2048; ++i)
            samples.push_back(static_cast<std::int16_t>((i * 37) - 40000));

        request.setAudio(protocol::AudioFormat{ 16000, 1 }, samples);
        return request;
    }

    // Свободный порт под фазу: берём первый, который сервер смог занять.
    // 0 = не заняли ни один.
    std::uint16_t listenFresh(NetworkServer& server, std::uint16_t firstPort)
    {
        for (std::uint16_t port = firstPort; port < firstPort + 20; ++port)
        {
            if (server.listen(std::string("tcp://127.0.0.1:") + std::to_string(port)))
                return port;
        }

        return 0;
    }
}

#define CHECK(expression) check((expression), #expression, __LINE__)

// ---------------------------------------------------------------------------
// Операции без connect()/listen()
// ---------------------------------------------------------------------------
static void testNotConnected(NetworkClient& client, NetworkServer& server)
{
    step("операции без connect()/listen() честно отказывают");

    CHECK(!client.receive());
    CHECK(client.lastError() == NetworkError::NotConnected);
    CHECK(!client.lastErrorText().empty());

    CHECK(!client.send(protocol::Request{}.toMessage()));
    CHECK(client.lastError() == NetworkError::NotConnected);

    CHECK(!server.receive());
    CHECK(server.lastError() == NetworkError::NotConnected);

    CHECK(!server.send(protocol::Request{}.toMessage()));
    CHECK(server.lastError() == NetworkError::NotConnected);

    report("client", client.lastError(), client.lastErrorText());
    report("server", server.lastError(), server.lastErrorText());
}

// ---------------------------------------------------------------------------
// Эхо: сердце шага
// ---------------------------------------------------------------------------
static void testEcho(NetworkClient& client, NetworkServer& server, std::uint16_t basePort)
{
    step("эхо: сервер возвращает отправленный Request без изменений");

    const auto port = listenFresh(server, basePort);
    CHECK(port != 0);
    if (port == 0)
        return;

    CHECK(client.connect("127.0.0.1", port));
    CHECK(client.isConnected());
    CHECK(server.isListening());

    const auto original = makeRequest(0xDEADBEEFu);
    CHECK(client.send(original.toMessage()));

    // Не const: ниже кадр переезжает в send() вместо того, чтобы копироваться.
    auto received = server.receive();
    CHECK(received.has_value());
    if (received)
    {
        const auto parsed = protocol::Request::fromMessage(*received);
        CHECK(parsed.has_value());
        CHECK(parsed && *parsed == original);

        // Эхо: отдаём ровно те же байты, что пришли.
        CHECK(server.send(std::move(*received)));
    }

    const auto echoed = client.receive();
    CHECK(echoed.has_value());
    if (echoed)
    {
        const auto parsed = protocol::Request::fromMessage(*echoed);
        CHECK(parsed.has_value());
        CHECK(parsed && *parsed == original);
    }

    CHECK(client.lastError() == NetworkError::Ok);
    report("client", client.lastError(), client.lastErrorText());
    report("server", server.lastError(), server.lastErrorText());
}

// ---------------------------------------------------------------------------
// Таймаут на сервере: клиент молчит, сервер не ждёт вечно
// ---------------------------------------------------------------------------
static void testServerTimeout(NetworkServer& server)
{
    step("таймаут: сервер не получил запрос за отведённое время");

    server.setReceiveTimeout(300);

    CHECK(!server.receive());
    CHECK(server.lastError() == NetworkError::Timeout);
    // Общее правило: ошибка закрыла сокет, дальше только listen() заново.
    CHECK(!server.isListening());

    server.setReceiveTimeout(5000); // вернуть норму для следующих фаз
    report("server", server.lastError(), server.lastErrorText());
}

// ---------------------------------------------------------------------------
// Обрыв: сервер ушёл, не ответив
// ---------------------------------------------------------------------------
static void testConnectionLost(NetworkClient& client, NetworkServer& server, std::uint16_t basePort)
{
    step("обрыв: сервер закрывается, не отвечая — клиент видит таймаут");

    const auto port = listenFresh(server, basePort);
    CHECK(port != 0);
    if (port == 0)
        return;

    CHECK(client.connect("127.0.0.1", port));

    const auto request = makeRequest(2);
    CHECK(client.send(request.toMessage()));

    const auto received = server.receive();
    CHECK(received.has_value()); // запрос дошёл...

    server.close(); // ...и сервер ушёл, не отвечая
    CHECK(!server.isListening());

    // zmq не выдаёт отдельного кода «пир пропал»: пропавший собеседник
    // просто перестаёт отвечать, и приём превращается в таймаут. Отсюда
    // в клиенте и таймаут, и обрыв ведут в одно событие CONNECTION_LOST.
    client.setReceiveTimeout(1500);

    const auto answer = client.receive();
    CHECK(!answer.has_value());
    CHECK(client.lastError() == NetworkError::Timeout);
    CHECK(!client.isConnected());

    report("client", client.lastError(), client.lastErrorText());

    // Проба на будущее (шаг 4): выйдет ли послушать тот же порт сразу после
    // close()? На Windows слушающий сокет libzmq берётся под
    // SO_EXCLUSIVEADDRUSE, и ответ зависит от того, остались ли на порту
    // TIME_WAIT-соединения. Печатаем, но не проверяем: результат зависит от
    // окружения, а проверять надо то, что всегда воспроизводится.
    const auto rebind = server.listen(std::string("tcp://127.0.0.1:") + std::to_string(port));
    std::printf("  переслушивание того же порта после close(): %s%s\n",
                rebind ? "ok" : "не вышло - ",
                rebind ? "" : server.lastErrorText().c_str());
    server.close();
}

// ---------------------------------------------------------------------------
// Таймаут на клиенте: сервер жив, но молчит
// ---------------------------------------------------------------------------
static void testClientTimeout(NetworkClient& client, NetworkServer& server, std::uint16_t basePort)
{
    step("таймаут: сервер принял запрос и не отвечает");

    const auto port = listenFresh(server, basePort);
    CHECK(port != 0);
    if (port == 0)
        return;

    CHECK(client.connect("127.0.0.1", port));

    // Меняем таймаут уже на живом сокете — значение должно примениться сразу.
    client.setReceiveTimeout(300);

    const auto request = makeRequest(3);
    CHECK(client.send(request.toMessage()));

    const auto received = server.receive();
    CHECK(received.has_value()); // принял и не отвечает

    const auto answer = client.receive();
    CHECK(!answer.has_value());
    CHECK(client.lastError() == NetworkError::Timeout);
    CHECK(!client.isConnected());

    report("client", client.lastError(), client.lastErrorText());
}

// ---------------------------------------------------------------------------
// Очерёдность REQ: второй запрос до ответа на первый
// ---------------------------------------------------------------------------
static void testRequestOrdering(NetworkClient& client, NetworkServer& server, std::uint16_t basePort)
{
    step("очерёдность REQ: второй запрос до ответа на первый — ошибка");

    const auto port = listenFresh(server, basePort);
    CHECK(port != 0);
    if (port == 0)
        return;

    CHECK(client.connect("127.0.0.1", port));

    const auto request = makeRequest(4);
    CHECK(client.send(request.toMessage()));
    CHECK(!client.send(request.toMessage())); // вот она, EFSM

    CHECK(client.lastError() == NetworkError::SocketError);
    CHECK(!client.isConnected());

    server.close();

    report("client", client.lastError(), client.lastErrorText());
}

int main()
{
    NetworkClient client;
    NetworkServer server;

    // Таймауты поменьше, чем в проде: зависшая фаза должна упасть за
    // секунды, а не держать ctest до своего лимита.
    client.setReceiveTimeout(5000);
    client.setSendTimeout(5000);
    server.setReceiveTimeout(5000);
    server.setSendTimeout(5000);

    testNotConnected(client, server);
    testEcho(client, server, 47001);
    testServerTimeout(server);                              // сервер закрыт
    testConnectionLost(client, server, 47011);
    testClientTimeout(client, server, 47021);               // сервер остался слушать
    testRequestOrdering(client, server, 47031);

    std::printf("transport echo: %d checks, %d failed\n", checksRun, checksFailed);
    return checksFailed == 0 ? 0 : 1;
}
