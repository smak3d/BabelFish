#pragma once

#include "BabelFish.h"
#include "State.h"
#include "Recorder.h"


class RecordingState : public State
{
    private:
        BabelFish& app;
        Recorder& recorder;
    public:
        RecordingState(BabelFish& app, Recorder& recorder);
        
        void enter() override;
        void update() override;
        void exit() override;
};
