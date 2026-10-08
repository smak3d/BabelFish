#include "SendingState.h"

#include "BabelFish.h"

#include <iostream>

SendingState::SendingState(
    BabelFish& app
)
    : app(app)
{
}

void SendingState::enter()
{
    std::cout << "Entering Sending State\n";
}

void SendingState::update()
{
    // Implementation for sending state
}

void SendingState::exit()
{
    // Implementation for exiting sending state
}

void SendingState::handleEvent(Event event)
{

}
