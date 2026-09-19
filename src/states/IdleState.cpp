#include "IdleState.h"
#include <iostream>

void IdleState::enter()
{
    std::cout << "Entering Idle State\n";
}

void IdleState::update()
{
}

void IdleState::exit()
{
    std::cout << "Starting recording...\n";
}
