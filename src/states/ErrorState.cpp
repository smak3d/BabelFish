#include "ErrorState.h"
#include <iostream>

void ErrorState::enter()
{
        switch (error)
    {
        case Error::UNKNOWN_ERROR:
            std::cout << "Unknown error\n";
            break;

        case Error::RECORDING_ERROR:
            std::cout << "Recording error\n";
            break;

        case Error::TRANSCRIPTION_ERROR:
            std::cout << "Transcription error\n";
            break;

        case Error::ASSISTANT_ERROR:
            std::cout << "Assistant error\n";
            break;
    }
}

void ErrorState::update()
{
    // Implementation for updating error state
}

void ErrorState::exit()
{
    // Implementation for exiting error state
}

void ErrorState::setError(Error error)
{
    this->error = error;
}
