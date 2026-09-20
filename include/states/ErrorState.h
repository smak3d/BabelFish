#pragma once

#include "State.h"
#include "../Error.h"


class ErrorState : public State
{
    private:
        Error error;

    public:
        void enter() override;
        void update() override;
        void exit() override;
        void handleEvent(Event event)override;

        void setError(Error error);
};