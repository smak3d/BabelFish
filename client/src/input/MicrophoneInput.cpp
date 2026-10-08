#include "MicrophoneInput.h"

#include "Recorder.h"

#include <windows.h>

MicrophoneInput::MicrophoneInput(Recorder& recorder)
    : recorder(recorder)
{
}

std::optional<Event> MicrophoneInput::update()
{
    // Тот же edge-детект Enter, что в KeyboardInput: тоггл, а не
    // «пока зажато» — см. комментарий там.
    const bool enterDown = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;
    const bool pressed    = enterDown && !enterWasDown;
    enterWasDown = enterDown;

    if (finished)
        return std::nullopt;

    if (capturing)
    {
        // Читаем сэмплы, пока запись идёт: Recorder сам решает, сколько
        // их есть, наша задача — не давать буферу отставать.
        recorder.update();
    }

    if (!pressed)
        return std::nullopt;

    if (!capturing)
    {
        recorder.startRecording();
        capturing = true;
        return Event::BUTTON_PRESSED;
    }

    // stopRecording() == false — пустой буфер или сбой PortAudio.
    // Событие всё равно BUTTON_RELEASED: InputState увидит пустые
    // data().audio и сам решит, что это ошибка ввода.
    recorder.stopRecording();

    const auto& recorded = recorder.getAudio();
    payload.audio.assign(recorded.begin(), recorded.end());

    capturing = false;
    finished  = true;
    return Event::BUTTON_RELEASED;
}

bool MicrophoneInput::isFinished() const
{
    return finished;
}

InputMode MicrophoneInput::mode() const
{
    return InputMode::Microphone;
}

const InputData& MicrophoneInput::data() const
{
    return payload;
}

void MicrophoneInput::reset()
{
    payload         = {};
    capturing    = false;
    finished     = false;
    enterWasDown = false;
}
