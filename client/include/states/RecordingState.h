#pragma once


#include "State.h"

class BabelFish;
class Recorder;



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
        void handleEvent(Event event)override;
};
