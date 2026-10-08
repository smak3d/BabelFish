#pragma once

#include "Input.h"

class Recorder;

// Микрофон как источник ввода: тоггл по Enter управляет уже готовым
// Recorder'ом (п.3 опирается на него, а не наращивает свой поток PortAudio):
//
//   BUTTON_PRESSED  -> recorder.startRecording()
//   пока активно     -> recorder.update() каждый кадр
//   BUTTON_RELEASED -> recorder.stopRecording(), сэмплы копируются
//                      в data().audio и уходят как AudioRequest.
//
// RecordingState делает ровно то же самое, но старт/стоп ему отдаёт
// BabelFish по событиям; здесь вся связка Enter -> Recorder живёт внутри
// источника, чтобы InputState не знал про PortAudio.
class MicrophoneInput : public Input
{
    public:
        // Recorder обязан жить дольше источника: тот только читает
        // его буфер и дёргает update() — своего потока у источника нет.
        explicit MicrophoneInput(Recorder& recorder);

        std::optional<Event> update() override;

        bool isFinished() const override;
        InputMode mode() const override;
        const InputData& data() const override;

        void reset() override;

    private:
        Recorder& recorder;

        // Поле названо payload, а не data: имя data занял метод data()
        // и компилятор не разводит поле и функцию одного имени.
        InputData payload;

        bool capturing  = false;   // запись идёт
        bool finished   = false;   // BUTTON_RELEASED отдан
        bool enterWasDown = false; // edge-детект Enter, как в KeyboardInput
};
