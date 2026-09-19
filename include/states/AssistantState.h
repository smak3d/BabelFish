#pragma once

#include "BabelFish.h"
#include "State.h"
#include "Assistant.h"

class AssistantState : public State
{
private:
    BabelFish& app;
    Assistant& assistant;

public:
    AssistantState(BabelFish& app, Assistant& assistant);

    void enter() override;
    void update() override;
    void exit() override;
};