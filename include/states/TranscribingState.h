#pragma once

#include "State.h"

class BabelFish;
class Transcriber;

class TranscribingState : public State
{
    private:
        BabelFish& app;
        Transcriber& transcriber;
    public:
        TranscribingState(BabelFish& app, Transcriber& transcriber);

        void enter() override;
        void update() override;
        void exit() override;
        void handleEvent(Event event)override;

};
