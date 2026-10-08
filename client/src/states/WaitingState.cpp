#include "WaitingState.h"

#include "BabelFish.h"

#include <iostream>

WaitingState::WaitingState(
    BabelFish& app
)
    : app(app)
{
}

void WaitingState::enter()
{
    std::cout << "Entering Waiting State\n";
}

void WaitingState::update()
{
    // Implementation for waiting state
}

void WaitingState::exit()
{
    // Implementation for exiting waiting state
}

void WaitingState::handleEvent(Event event)
{

}
