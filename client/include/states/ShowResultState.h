#pragma once

#include "State.h"

class BabelFish;

// Состояние показа результата: печатает транскрипцию и ответ ассистента
// (то, что WaitingState положил в приложение) и закрывает цикл событием
// RESULT_SHOWN — конвейер возвращается в IdleState за следующим запросом.
class ShowResultState : public State
{
    private:
        BabelFish& app;
    public:
        ShowResultState(BabelFish& app);

        void enter() override;
        void update() override;
        void exit() override;
        void handleEvent(Event event)override;
};
