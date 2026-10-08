#pragma once

#include "AudioFormat.h"
#include "Input.h"
#include "Request.h"

#include <cstdint>
#include <span>
#include <string>

// Сборка protocol::Request (п.1) из того, что собрал Input (п.3).
//
// RequestBuilder — единственное место клиента, где знают, во что
// превращаются данные ввода:
//
//   InputMode::Microphone -> AudioRequest: PCM16 сэмплы, частота/каналы
//                            уезжают в Request.audioFormat — тем самым
//                            полем, которое в п.1 вынесли из Request,
//                            чтобы формат ехал вместе с данными.
//   InputMode::Keyboard   -> TextRequest: UTF-8, audioFormat нулевой
//                            и ни на что не влияет.
//
// id запроса проставляется здесь: он сопоставляет запрос с ответом (п.5
// будет ждать Response с тем же id), поэтому монотонный счётчик —
// часть упаковки, а не сетевого слоя.
//
// Новый режим ввода = новая ветка в build(). Каркас Request (заголовок
// + audioFormat + payload) не меняется.
class RequestBuilder
{
    public:
        // Формат аудио обязан совпадать с Recorder: builder не спрашивает
        // у Recorder его sampleRate/channels, поэтому частота/каналы
        // передаются сюда — и лучше брать их из того же места, где
        // создаётся Recorder (см. BabelFish: recorder(16000, 1)).
        RequestBuilder(std::uint32_t sampleRate, std::uint16_t channels);

        // Собрать запрос по режиму данных. Возвращает nullopt, если
        // данные не готовы (input.isFinished() ещё false) или пусты —
        // пустой запрос на сервер отправлять нечего. Счётчик id
        // двигается: каждый собранный запрос получает новый id.
        std::optional<protocol::Request> build(Input& input);

        // Прямые сборки для случаев, когда Input не участвует
        // (п.5: SendingState может пересобрать или дослать запрос).
        protocol::Request buildAudio(std::span<const std::int16_t> samples);
        protocol::Request buildText(const std::string& text);

        // id, которое получит следующий запрос.
        std::uint32_t nextId() const;

        // Формат аудио, который уезжает в Request.audioFormat.
        // Геттер со словом get — как getCurrentState/getAudio в BabelFish:
        // поле format и геттер format() компилятор не разводит.
        const protocol::AudioFormat& getFormat() const;

    private:
        protocol::AudioFormat format;

        // Счётчик начинается с 1: id == 0 зарезервирован как «значение
        // по умолчанию» в protocol::Request, путать его с настоящим
        // запросом нельзя.
        std::uint32_t lastId = 0;
};
