#include "ShowResultState.h"

#include "BabelFish.h"

#include <iostream>

ShowResultState::ShowResultState(
    BabelFish& app
)
    : app(app)
{
}

void ShowResultState::enter()
{
    std::cout << "Entering Show Result State\n";
}

void ShowResultState::update()
{
    // Implementation for show result state
}

void ShowResultState::exit()
{
    // Implementation for exiting show result state
}

void ShowResultState::handleEvent(Event event)
{

}
