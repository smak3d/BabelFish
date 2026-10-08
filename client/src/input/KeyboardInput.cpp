#include "KeyboardInput.h"

#include <conio.h>
#include <windows.h>

std::optional<Event> KeyboardInput::update()
{
    // Edge по фронту: событие даём в момент нажатия, а не пока клавиша
    // зажата — иначе автоповтор Enter переключал бы тоггл несколько раз
    // за одно удержание.
    const bool enterDown = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;
    const bool pressed    = enterDown && !enterWasDown;
    enterWasDown = enterDown;

    if (finished)
    {
        drainConsole(); // буфер консоли надо продолжать чистить, иначе
                        // после финального Enter он забьётся мусором
        return std::nullopt;
    }

    // Текст копим каждый кадр — независимо от того, было ли нажатие,
    // чтобы не терять символы, набранные между опросами.
    drainConsole();

    if (!pressed)
        return std::nullopt;

    if (!capturing)
    {
        capturing = true;
        return Event::BUTTON_PRESSED;
    }

    capturing = false;
    finished  = true;
    return Event::BUTTON_RELEASED;
}

bool KeyboardInput::isFinished() const
{
    return finished;
}

InputMode KeyboardInput::mode() const
{
    return InputMode::Keyboard;
}

const InputData& KeyboardInput::data() const
{
    return payload;
}

void KeyboardInput::reset()
{
    payload        = {};
    capturing   = false;
    finished    = false;
    enterWasDown = false;
}

void KeyboardInput::drainConsole()
{
    while (_kbhit())
    {
        const int key = _getch();

        // Расширенные клавиши (стрелки, F-клавиши) приходят двумя
        // кодами: 0/0xE0 плюс скан-код. В текст они не идут —
        // второй код тоже забираем, чтобы не оставался в буфере.
        if (key == 0 || key == 0xE0)
        {
            if (_kbhit())
                _getch();
            continue;
        }

        // Enter живёт в тоггле (GetAsyncKeyState), в текст не попадает.
        if (key == '\r' || key == '\n')
            continue;

        // Backspace: без него «забивка руками» непригодна.
        if (key == '\b')
        {
            if (!payload.text.empty())
                payload.text.pop_back();
            continue;
        }

        if (!capturing || finished)
            continue;

        payload.text += static_cast<char>(key);
    }
}
