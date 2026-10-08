#pragma once

#include "State.h"
#include "Event.h"

#include "Recorder.h"

#include "ErrorState.h"
#include "IdleState.h"
#include "RecordingState.h"
#include "SendingState.h"
#include "WaitingState.h"
#include "ShowResultState.h"

#include <vector>
#include <cstdint>
#include <string>

class BabelFish
{
    private:
        State* currentState;

        Recorder recorder;

        ErrorState errorState;
        IdleState idleState;
        RecordingState recordingState;
        SendingState sendingState;
        WaitingState waitingState;
        ShowResultState showResultState;

        std::vector<int16_t> audio;
        std::string transcription;
        std::string response;

    public:
        BabelFish();

        void update();
        void handleEvent(Event event, bool success);
        void changeState(State* newState);

        State* getCurrentState();

        std::vector<int16_t>& getAudio();
        void setAudio(const std::vector<int16_t>& audio);
        std::string& getTranscription();
        void setTranscription(const std::string& text);
        std::string& getResponse();
        void setResponse(const std::string& text);

};
