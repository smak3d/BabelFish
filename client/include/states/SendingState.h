#pragma once

#include "State.h"

class BabelFish;

class SendingState : public State
{
    private:
        BabelFish& app;
    public:
        SendingState(BabelFish& app);

        void enter() override;
        void update() override;
        void exit() override;
        void handleEvent(Event event)override;
};
