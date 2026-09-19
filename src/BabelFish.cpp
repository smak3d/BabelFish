#pragma once

#include "BabelFish.h"


BabelFish::BabelFish(State* initialState) 
    : recorder(16000, 1),
      transcriber(16000),
      assistant(),
      recordingState(*this, recorder),
      transcribingState(*this, transcriber),
      assistantState(*this, assistant),
      currentState(initialState)
{
    currentState->enter();
}

void BabelFish::update()
{
    currentState->update();
}

void BabelFish::handleEvent(Event event, bool success)
{
    if(!success)
    {
        switch(event)
        {
            case Event::BUTTON_PRESSED:
            case Event::BUTTON_RELEASED:
                
                errorState.setError(Error::RECORDING_ERROR);
                break;
            case Event::TRANSCRIPTION_FINISHED:
            
                errorState.setError(Error::TRANSCRIPTION_ERROR);
                break;
            case Event::ASSISTANT_FINISHED:
                
                errorState.setError(Error::ASSISTANT_ERROR);
                break;
        }
        changeState(&errorState);
        return;
    }

    switch(event)
    {
        case Event::BUTTON_PRESSED:
            changeState(&recordingState);
            break;
        case Event::BUTTON_RELEASED:
            changeState(&transcribingState);
            break;
        case Event::TRANSCRIPTION_FINISHED:
            changeState(&assistantState);
            break;
        case Event::ASSISTANT_FINISHED:
            changeState(&idleState);
            break;
    }
    
}

void BabelFish::changeState(State* newState)
{
    if (currentState)
    {
        currentState->exit();
    }
    currentState = newState;
    if (currentState)
    {
        currentState->enter();
    }
}

State* BabelFish::getCurrentState()
{
    return currentState;
}

std::vector<int16_t>&BabelFish::getAudio()
{
    return audio;
}
void BabelFish::setAudio(const std::vector<int16_t>& audio)
{
    this->audio = audio;
}

std::string& BabelFish::getTranscription()
{
    return transcription;
}

void BabelFish::setTranscription(const std::string& text)
{
    this->transcription = text;
}


std::string& BabelFish::getResponse()
{
    return response;
}

void BabelFish::setResponse(const std::string& text)
{
    this->response = text;
}