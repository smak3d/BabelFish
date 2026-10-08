#pragma once

#include "State.h"

class BabelFish;

// Состояние ожидания: приложение готово к новому циклу, соединение с
// сервером на месте. Старт цикла (BUTTON_PRESSED) раздаёт main.cpp —
// по нему цепочка уходит в RECORDING: InputState для клавиатуры либо
// RecordingState для микрофона (см. BabelFish.h).
class IdleState : public State
{
    private:
        BabelFish& app;

    public:
        IdleState(BabelFish& app);

        void enter() override;
        void update() override;
        void exit() override;
        void handleEvent(Event event) override;
};
