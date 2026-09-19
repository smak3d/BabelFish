#pragma once

#include "State.h"

class IdleState : public State
{
    public:
        void enter() override;
        void update() override;
        void exit() override;

};
