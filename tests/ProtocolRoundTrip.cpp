// Round-trip тест протокола: сериализация -> десериализация обязана вернуть
// то же самое, а битые, обрезанные и «чужие» кадры — отвергаться.
//
// Ни zmq-сокетов, ни winsock здесь нет: проверяется только формат.

#include "AudioFormat.h"
#include "MessageType.h"
#include "Request.h"
#include "Response.h"

#include <cstdint>
#include <cstdio>
#include <span>
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

    std::vector<std::byte> toBytes(const std::string& value)
    {
        const auto* begin = reinterpret_cast<const std::byte*>(value.data());
        return std::vector<std::byte>(begin, begin + value.size());
    }

    std::span<const std::byte> prefix(const std::vector<std::byte>& bytes, std::size_t size)
    {
        return { bytes.data(), size };
    }
}

#define CHECK(expression) check((expression), #expression, __LINE__)

// ---------------------------------------------------------------------------
// AudioFormat
// ---------------------------------------------------------------------------
static void testAudioFormat()
{
    const protocol::AudioFormat format{ 16000, 1 };
    const auto bytes = format.serialize();

    CHECK(bytes.size() == protocol::AudioFormat::wireSize);

    // little-endian обязан быть явным, а не совпадением с архитектурой:
    // 16000 = 0x3E80 -> 80 3E 00 00, каналы 1 -> 01 00
    CHECK(static_cast<std::uint8_t>(bytes[0]) == 0x80);
    CHECK(static_cast<std::uint8_t>(bytes[1]) == 0x3E);
    CHECK(static_cast<std::uint8_t>(bytes[2]) == 0x00);
    CHECK(static_cast<std::uint8_t>(bytes[3]) == 0x00);
    CHECK(static_cast<std::uint8_t>(bytes[4]) == 0x01);
    CHECK(static_cast<std::uint8_t>(bytes[5]) == 0x00);

    const auto parsed = protocol::AudioFormat::deserialize(bytes);
    CHECK(parsed.has_value());
    CHECK(parsed && *parsed == format);

    // Обрезанный буфер не должен давать полукадр.
    CHECK(!protocol::AudioFormat::deserialize(prefix(bytes, 5)).has_value());
    CHECK(!protocol::AudioFormat::deserialize(prefix(bytes, 0)).has_value());
}

// ---------------------------------------------------------------------------
// Request с аудио
// ---------------------------------------------------------------------------
static void testRequestAudio()
{
    const protocol::AudioFormat format{ 16000, 1 };

    std::vector<std::int16_t> samples;
    samples.reserve(4096);
    for (int i = 0; i < 4096; ++i)
        samples.push_back(static_cast<std::int16_t>((i * 37) - 18000));

    protocol::Request request;
    request.id = 0xDEADBEEFu;
    request.setAudio(format, samples);

    CHECK(request.type == protocol::MessageType::AudioRequest);
    CHECK(request.audioFormat == format);
    CHECK(request.payload.size() == samples.size() * sizeof(std::int16_t));

    const auto bytes  = request.serialize();
    const auto parsed = protocol::Request::deserialize(bytes);
    CHECK(parsed.has_value());
    if (!parsed)
        return;

    CHECK(parsed->id == request.id);
    CHECK(parsed->type == request.type);
    CHECK(parsed->audioFormat == format);
    CHECK(parsed->payload == request.payload);
    CHECK(*parsed == request);

    const auto parsedSamples = parsed->getAudio();
    CHECK(parsedSamples.has_value());
    CHECK(parsedSamples && *parsedSamples == samples);

    // Оборванный префикс кадра = отклонение, а не полукадр.
    for (std::size_t size = 0; size < protocol::headerSize + protocol::AudioFormat::wireSize; ++size)
        CHECK(!protocol::Request::deserialize(prefix(bytes, size)).has_value());
}

// ---------------------------------------------------------------------------
// Request с текстовым payload: аудио тут ни при чём, формат нулевой
// ---------------------------------------------------------------------------
static void testRequestText()
{
    protocol::Request request;
    request.id   = 42;
    request.type = protocol::MessageType::TextRequest;
    request.payload = toBytes("Привет, мир! 🎧");

    const auto bytes  = request.serialize();
    const auto parsed = protocol::Request::deserialize(bytes);
    CHECK(parsed.has_value());
    if (!parsed)
        return;

    CHECK(parsed->id == request.id);
    CHECK(parsed->type == protocol::MessageType::TextRequest);
    CHECK(parsed->payload == request.payload);
    CHECK(parsed->audioFormat == protocol::AudioFormat{});
    // Нечётное число байт = сэмпл порван посередине -> битый кадр.
    CHECK(!parsed->getAudio().has_value());
}

// ---------------------------------------------------------------------------
// Ответ
// ---------------------------------------------------------------------------
static void testResponse()
{
    protocol::Response response;
    response.id            = 7;
    response.status        = protocol::ResponseStatus::TranscriptionError;
    response.transcription = "Распознано: «привет»";
    response.response      = "";

    const auto bytes  = response.serialize();
    const auto parsed = protocol::Response::deserialize(bytes);
    CHECK(parsed.has_value());
    if (!parsed)
        return;

    CHECK(*parsed == response);
    CHECK(!parsed->ok());
    CHECK(parsed->id == 7);
    CHECK(parsed->transcription == response.transcription);
    CHECK(parsed->response.empty());

    // Успешный ответ с непустым ответом ассистента.
    protocol::Response ok;
    ok.id            = 8;
    ok.status        = protocol::ResponseStatus::Ok;
    ok.transcription = "";
    ok.response      = "Ответ ассистента";

    const auto okParsed = protocol::Response::deserialize(ok.serialize());
    CHECK(okParsed.has_value());
    CHECK(okParsed && *okParsed == ok);
    CHECK(okParsed && okParsed->ok());

    // Обрезанные длины строк не должны превращаться в обрывки строк.
    for (std::size_t size = 0; size < protocol::headerSize + 1 + 4; ++size)
        CHECK(!protocol::Response::deserialize(prefix(bytes, size)).has_value());
}

// ---------------------------------------------------------------------------
// Кадр одного типа не читается как кадр другого
// ---------------------------------------------------------------------------
static void testCrossTypeRejection()
{
    protocol::Response response;
    response.id            = 7;
    response.status        = protocol::ResponseStatus::Ok;
    response.transcription = "текст";
    response.response      = "ответ";

    protocol::Request request;
    request.id   = 7;
    request.type = protocol::MessageType::TextRequest;
    request.payload = toBytes("текст");

    CHECK(!protocol::Request::deserialize(response.serialize()).has_value());
    CHECK(!protocol::Response::deserialize(request.serialize()).has_value());
}

// ---------------------------------------------------------------------------
// Повреждённая версия протокола
// ---------------------------------------------------------------------------
static void testVersionMismatch()
{
    protocol::Request request;
    request.id   = 1;
    request.type = protocol::MessageType::RawRequest;
    request.payload = { std::byte{0x01}, std::byte{0x02}, std::byte{0x03} };

    auto bytes = request.serialize();
    bytes[0] = static_cast<std::byte>(protocol::version + 1);

    CHECK(!protocol::Request::deserialize(bytes).has_value());
}

// ---------------------------------------------------------------------------
// Превью заголовка: по типу видно, запрос это или ответ
// ---------------------------------------------------------------------------
static void testHeaderPeek()
{
    const protocol::MessageHeader header{ protocol::MessageType::AudioRequest, 99 };
    const auto bytes  = header.serialize();
    const auto parsed = protocol::MessageHeader::deserialize(bytes);

    CHECK(parsed.has_value());
    CHECK(parsed && parsed->type == protocol::MessageType::AudioRequest);
    CHECK(parsed && parsed->id == 99);
    CHECK(parsed && protocol::isRequest(parsed->type));
    CHECK(!protocol::isResponse(protocol::MessageType::AudioRequest));
    CHECK(protocol::isResponse(protocol::MessageType::Result));
    CHECK(!protocol::isRequest(protocol::MessageType::Result));
}

// ---------------------------------------------------------------------------
// zmq-обёртка: сообщение обязано пережить упаковку без искажений
// ---------------------------------------------------------------------------
static void testZmqMessage()
{
    const protocol::AudioFormat format{ 48000, 2 };

    std::vector<std::int16_t> samples;
    for (int i = 0; i < 512; ++i)
        samples.push_back(static_cast<std::int16_t>(i - 256));

    protocol::Request request;
    request.id = 1234;
    request.setAudio(format, samples);

    const auto message = request.toMessage();
    CHECK(message.size() == request.serialize().size());

    const auto parsed = protocol::Request::fromMessage(message);
    CHECK(parsed.has_value());
    CHECK(parsed && *parsed == request);

    protocol::Response response;
    response.id            = 1234;
    response.status        = protocol::ResponseStatus::Ok;
    response.transcription = "привет";
    response.response      = "мир";

    const auto responseMessage = response.toMessage();
    const auto parsedResponse  = protocol::Response::fromMessage(responseMessage);
    CHECK(parsedResponse.has_value());
    CHECK(parsedResponse && *parsedResponse == response);
}

int main()
{
    testAudioFormat();
    testRequestAudio();
    testRequestText();
    testResponse();
    testCrossTypeRejection();
    testVersionMismatch();
    testHeaderPeek();
    testZmqMessage();

    std::printf("protocol round-trip: %d checks, %d failed\n", checksRun, checksFailed);
    return checksFailed == 0 ? 0 : 1;
}
