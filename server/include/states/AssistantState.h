#pragma once


#include "State.h"

class BabelFish;
class Assistant;

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
    void handleEvent(Event event)override;
};