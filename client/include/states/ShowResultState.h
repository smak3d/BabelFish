#pragma once

#include "State.h"

class BabelFish;

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
