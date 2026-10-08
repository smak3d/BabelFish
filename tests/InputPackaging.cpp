// Рџ.3 (РІС…РѕРґ РєР»РёРµРЅС‚Р° + СѓРїР°РєРѕРІРєР°): Р·Р°РїРёСЃР°РЅРЅС‹Р№ СЃРёРіРЅР°Р» -> РіРѕС‚РѕРІС‹Р№ Р±Р°Р№С‚РѕРІС‹Р№
// Р·Р°РїСЂРѕСЃ. РџСЂРѕРІРµСЂСЏРµС‚СЃСЏ С†РµРїРѕС‡РєР°
//
//     Input (РґР°РЅРЅС‹Рµ) -> RequestBuilder -> protocol::Request -> Р±Р°Р№С‚С‹
//
// Р±РµР· PortAudio Рё СЃРѕРєРµС‚РѕРІ: Input РїРѕРґРјРµРЅСЏРµС‚СЃСЏ С„РµР№РєРѕРј, Recorder РЅРµ РЅСѓР¶РµРЅ вЂ”
// RequestBuilder РІРёРґРёС‚ С‚РѕР»СЊРєРѕ РёРЅС‚РµСЂС„РµР№СЃ Input, РєР°Рє Рё InputState.

#include "Input.h"
#include "RequestBuilder.h"

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

    // Р¤РµР№РєРѕРІС‹Р№ Input: РґР°РЅРЅС‹Рµ Р·Р°РґР°СЋС‚СЃСЏ СЃРЅР°СЂСѓР¶Рё, update() РЅРёС‡РµРіРѕ РЅРµ РѕРїСЂР°С€РёРІР°РµС‚.
    class FakeInput : public Input
    {
        public:
            FakeInput(InputMode mode, InputData data, bool finished)
                : mode_(mode), data_(std::move(data)), finished_(finished)
            {
            }

            std::optional<Event> update() override { return std::nullopt; }
            bool isFinished() const override { return finished_; }
            InputMode mode() const override { return mode_; }
            const InputData& data() const override { return data_; }
            void reset() override { data_ = {}; finished_ = false; }

        private:
            InputMode mode_;
            InputData data_;
            bool finished_;
    };

    std::vector<std::int16_t> recordedSamples()
    {
        std::vector<std::int16_t> samples;
        samples.reserve(4096);
        for (int i = 0; i < 4096; ++i)
            samples.push_back(static_cast<std::int16_t>((i * 37) - 18000));
        return samples;
    }
}

#define CHECK(expression) check((expression), #expression, __LINE__)

// ---------------------------------------------------------------------------
// Р—Р°РїРёСЃР°РЅРЅС‹Р№ СЃРёРіРЅР°Р» -> AudioRequest -> Р±Р°Р№С‚С‹ -> С‚РѕС‚ Р¶Рµ СЃРёРіРЅР°Р»
// ---------------------------------------------------------------------------
static void testRecordedSignalToBytes()
{
    RequestBuilder builder(16000, 1); // РєР°Рє recorder(16000, 1) РІ BabelFish

    const auto samples = recordedSamples();

    InputData data;
    data.audio = samples;

    FakeInput input(InputMode::Microphone, data, true /* РіРѕС‚РѕРІ */);
    const auto request = builder.build(input);

    CHECK(request.has_value());
    if (!request)
        return;

    CHECK(request->type == protocol::MessageType::AudioRequest);
    // Лишние скобки вокруг выражения: запятая в AudioFormat{ 16000, 1 }
    // иначе воспринимается макросом как граница аргументов CHECK.
    CHECK((request->audioFormat == protocol::AudioFormat{ 16000, 1 }));
    CHECK(request->id == 1);
    CHECK(request->payload.size() == samples.size() * sizeof(std::int16_t));

    // Р“РѕС‚РѕРІС‹Р№ Р±Р°Р№С‚РѕРІС‹Р№ Р·Р°РїСЂРѕСЃ: С‚Рѕ, С‡С‚Рѕ СѓР№РґС‘С‚ РїРѕ СЃРѕРєРµС‚Сѓ (Рї.5).
    const auto bytes = request->serialize();
    CHECK(!bytes.empty());

    // РћР±СЂР°С‚РЅР°СЏ РґРѕСЂРѕРіР°: СЃРµСЂРІРµСЂ РёР· СЌС‚РёС… Р±Р°Р№С‚РѕРІ РѕР±СЏР·Р°РЅ РїРѕР»СѓС‡РёС‚СЊ С‚Рµ Р¶Рµ СЃСЌРјРїР»С‹.
    const auto parsed = protocol::Request::deserialize(bytes);
    CHECK(parsed.has_value());
    if (!parsed)
        return;

    CHECK(*parsed == *request);

    const auto parsedSamples = parsed->getAudio();
    CHECK(parsedSamples.has_value());
    CHECK(parsedSamples && *parsedSamples == samples);
}

// ---------------------------------------------------------------------------
// РљР»Р°РІРёР°С‚СѓСЂРЅС‹Р№ РІРІРѕРґ -> TextRequest, С„РѕСЂРјР°С‚ Р°СѓРґРёРѕ РЅСѓР»РµРІРѕР№
// ---------------------------------------------------------------------------
static void testKeyboardTextToBytes()
{
    RequestBuilder builder(16000, 1);

    InputData data;
    data.text = "РїСЂРёРІРµС‚ РёР· BabelFish";

    FakeInput input(InputMode::Keyboard, data, true);
    const auto request = builder.build(input);

    CHECK(request.has_value());
    if (!request)
        return;

    CHECK(request->type == protocol::MessageType::TextRequest);
    CHECK(request->audioFormat == protocol::AudioFormat{});
    CHECK(!request->getAudio().has_value()); // СЌС‚Рѕ РЅРµ Р°СѓРґРёРѕ

    const auto parsed = protocol::Request::deserialize(request->serialize());
    CHECK(parsed.has_value());
    CHECK(parsed && *parsed == *request);
}

// ---------------------------------------------------------------------------
// Р“РѕС‚РѕРІРЅРѕСЃС‚Рё РЅРµС‚ РёР»Рё РґР°РЅРЅС‹Рµ РїСѓСЃС‚С‹ -> Р·Р°РїСЂРѕСЃР° РЅРµС‚
// ---------------------------------------------------------------------------
static void testNotReadyAndEmpty()
{
    RequestBuilder builder(16000, 1);

    InputData data;
    data.audio = recordedSamples();

    // Р’РІРѕРґ РµС‰С‘ РёРґС‘С‚ вЂ” builder РЅРµ РґРѕР»Р¶РµРЅ СЃРѕР±РёСЂР°С‚СЊ РЅР°РїРѕР»РѕРІРёРЅСѓ Р·Р°РїРёСЃР°РЅРЅРѕРµ.
    FakeInput inProgress(InputMode::Microphone, data, false);
    CHECK(!builder.build(inProgress).has_value());

    // Р’РІРѕРґ Р·Р°РІРµСЂС€С‘РЅ, РЅРѕ РґР°РЅРЅС‹С… РЅРµС‚ (Enter РјРіРЅРѕРІРµРЅРЅРѕ / РїСѓСЃС‚РѕР№ Р±СѓС„РµСЂ).
    InputData empty;
    FakeInput emptyMic(InputMode::Microphone, empty, true);
    CHECK(!builder.build(emptyMic).has_value());

    FakeInput emptyText(InputMode::Keyboard, empty, true);
    CHECK(!builder.build(emptyText).has_value());
}

// ---------------------------------------------------------------------------
// id Р·Р°РїСЂРѕСЃРѕРІ РјРѕРЅРѕС‚РѕРЅРЅРѕ СЂР°СЃС‚СѓС‚: РѕС‚РІРµС‚ (Рї.1) СЃРѕРїРѕСЃС‚Р°РІР»СЏРµС‚СЃСЏ РїРѕ id,
// РґРІР° РѕРґРёРЅР°РєРѕРІС‹С… id СЃР»РѕРјР°Р»Рё Р±С‹ РѕР¶РёРґР°РЅРёРµ РІ WaitingState (Рї.5)
// ---------------------------------------------------------------------------
static void testIdsIncrease()
{
    RequestBuilder builder(16000, 1);

    CHECK(builder.nextId() == 1);

    const auto first = builder.buildAudio(std::span<const std::int16_t>{});
    CHECK(first.id == 1);
    CHECK(builder.nextId() == 2);

    const auto second = builder.buildText("РІС‚РѕСЂРѕР№");
    CHECK(second.id == 2);
    CHECK(builder.nextId() == 3);

    const auto third = builder.buildText("С‚СЂРµС‚РёР№");
    CHECK(third.id == 3);

    CHECK((builder.getFormat() == protocol::AudioFormat{ 16000, 1 }));
}

// ---------------------------------------------------------------------------
// reset() InputState: InputState СЃР±СЂР°СЃС‹РІР°РµС‚ input РІ enter() вЂ” РґР°РЅРЅС‹Рµ
// РїСЂРѕС€Р»РѕРіРѕ С†РёРєР»Р° РЅРµ РґРѕР»Р¶РЅС‹ РїРµСЂРµР¶РёС‚СЊ РЅРѕРІС‹Р№
// ---------------------------------------------------------------------------
static void testInputReset()
{
    InputData data;
    data.text = "СЃС‚Р°СЂС‹Р№ С‚РµРєСЃС‚";

    FakeInput input(InputMode::Keyboard, data, true);
    CHECK(input.isFinished());
    CHECK(!input.data().text.empty());

    input.reset();
    CHECK(!input.isFinished());
    CHECK(input.data().text.empty());
    CHECK(input.data().audio.empty());
}

int main()
{
    testRecordedSignalToBytes();
    testKeyboardTextToBytes();
    testNotReadyAndEmpty();
    testIdsIncrease();
    testInputReset();

    std::printf("input packaging: %d checks, %d failed\n", checksRun, checksFailed);
    return checksFailed == 0 ? 0 : 1;
}
