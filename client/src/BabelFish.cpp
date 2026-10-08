#include "BabelFish.h"


BabelFish::BabelFish() 
    : recorder(16000, 1),
      recordingState(*this, recorder),
      sendingState(*this),
      waitingState(*this),
      showResultState(*this),
      currentState(&idleState)
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
            case Event::RECORDING_FINISHED:
                errorState.setError(Error::RECORDING_ERROR);
                break;
            case Event::SENDING_FINISHED:
            case Event::RESPONSE_RECEIVED:
                errorState.setError(Error::NETWORK_ERROR);
                break;
            case Event::RESULT_SHOWN:
                errorState.setError(Error::UNKNOWN_ERROR);
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
            break;
        case Event::RECORDING_FINISHED:
            changeState(&sendingState);
            break;
        case Event::SENDING_FINISHED:
            changeState(&waitingState);
            break;
        case Event::RESPONSE_RECEIVED:
            changeState(&showResultState);
            break;
        case Event::RESULT_SHOWN:
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