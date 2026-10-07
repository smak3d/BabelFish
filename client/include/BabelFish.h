#pragma once

#include "State.h"
#include "Event.h"

#include "Recorder.h"
#include "Transcriber.h"
#include "Assistant.h"

#include "ErrorState.h"
#include "IdleState.h"
#include "RecordingState.h"
#include "TranscribingState.h"
#include "AssistantState.h"

#include <vector>
#include <cstdint>
#include <string>

class BabelFish
{
    private:
        State* currentState;

        Recorder recorder;
        Transcriber transcriber;
        Assistant assistant;

        ErrorState errorState;
        IdleState idleState;
        RecordingState recordingState;
        TranscribingState transcribingState;
        AssistantState assistantState;

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