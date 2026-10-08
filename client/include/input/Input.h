#pragma once

#include "Event.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Режим входа — выбор пользователя до старта. От него зависит, какое
// состояние автомата собирает данные и каким MessageType они уйдут (п.1):
// Keyboard -> InputState, Microphone -> RecordingState.
// Новый режим (картинка, файл...) = новый InputMode + новый MessageType,
// каркас ниже не меняется.
enum class InputMode
{
    Keyboard,   // текст, забитый руками  -> TextRequest
    Microphone  // записанный сигнал     -> AudioRequest
};

// Данные, которые собрал Input. Заполнено ровно то поле, которое
// соответствует mode(): остальное пустое.
struct InputData
{
    std::string text;                  // InputMode::Keyboard
    std::vector<std::int16_t> audio;   // InputMode::Microphone
};

// Источник входных данных клиента.
//
// Контракт общий для KeyboardInput и MicrophoneInput:
//
//   - update() опрашивает устройство за один кадр и возвращает событие,
//     когда «кнопка» переключилась. Кнопка тоггловая, как два cin.get()
//     в main.cpp, только без блокировки: первое нажатие Enter ->
//     BUTTON_PRESSED, следующее -> BUTTON_RELEASED (edge по фронту,
//     автоповтор удержания второй раз не считается).
//
//   - между BUTTON_PRESSED и BUTTON_RELEASED Input копит данные
//     (data()); после BUTTON_RELEASED isFinished() == true и данные
//     можно отдавать в RequestBuilder.
//
//   - reset() возвращает источник к исходному состоянию: InputState
//     зовёт его в enter(), чтобы каждый цикл начинался заново.
//
// Ввод опрашивается, а не блокирует: update() обязан возвращаться
// сразу, иначе держится кадр всего автомата (см. update() в main.cpp).
class Input
{
    public:
        virtual ~Input() = default;

        // Опрос за кадр: nullopt = «ничего не произошло».
        virtual std::optional<Event> update() = 0;

        // Данные собраны, RequestBuilder можно звать.
        virtual bool isFinished() const = 0;

        virtual InputMode mode() const = 0;

        // Собранные данные. До isFinished() могут быть неполными.
        virtual const InputData& data() const = 0;

        // Сброс к исходному состоянию.
        virtual void reset() = 0;
};
