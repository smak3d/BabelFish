#pragma once

#include "State.h"

class BabelFish;

class WaitingState : public State
{
    private:
        BabelFish& app;
    public:
        WaitingState(BabelFish& app);

        void enter() override;
        void update() override;
        void exit() override;
        void handleEvent(Event event)override;
};
