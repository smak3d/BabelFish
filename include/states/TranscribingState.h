#pragma once

#include "BabelFish.h"
#include "State.h"
#include "Transcriber.h"

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

};
