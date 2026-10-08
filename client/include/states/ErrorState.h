#pragma once

#include "State.h"
#include "../Error.h"

class BabelFish;

// Состояние восстановления: после любой ошибки цикл закольцовывается
// через IdleState. Если упала сеть, сокет уже закрыт (контракт
// NetworkClient), поэтому ErrorState ставит connect() заново — на том же
// host/port. Ошибки ввода и моделей соединение не трогают, но
// ensureConnected() в любом случае проверяет, что сокет жив.
class ErrorState : public State
{
    private:
        BabelFish& app;
        Error error = Error::UNKNOWN_ERROR;

    public:
        ErrorState(BabelFish& app);

        void enter() override;
        void update() override;
        void exit() override;
        void handleEvent(Event event)override;

        void setError(Error error);
};
