#pragma once

#include "Input.h"

// Клавиатура в роли «кнопки» и источника текста.
//
// Тоггл по Enter: первое нажатие -> BUTTON_PRESSED и дальше печатается
// текст, следующее -> BUTTON_RELEASED и текст фиксируется — это и есть
// то, что «забили руками» и что уйдёт на сервер как TextRequest.
//
// Нажатие ловится по фронту GetAsyncKeyState(VK_RETURN), а не по символу
// в буфере консоли: символом удержание Enter выглядит как поток '\r'
// (автоповтор) и тоггл мигал бы. Символы при этом читаются отдельно,
// через _kbhit() — неблокирующе, чтобы update() не подвешивал кадр.
class KeyboardInput : public Input
{
    public:
        std::optional<Event> update() override;

        bool isFinished() const override;
        InputMode mode() const override;
        const InputData& data() const override;

        void reset() override;

    private:
        // Забрать накопленные в буфере консоли символы в data.text.
        // Вызывается каждый кадр; текст копится только пока capturing.
        void drainConsole();

        // Поле названо payload, а не data: имя data занял метод data()
        // и компилятор не разводит поле и функцию одного имени.
        InputData payload;

        bool capturing  = false;  // между BUTTON_PRESSED и BUTTON_RELEASED
        bool finished   = false;  // BUTTON_RELEASED отдан, данных больше не будет
        bool enterWasDown = false; // предыдущее состояние Enter для edge-детекта
};
