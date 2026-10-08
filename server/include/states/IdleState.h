#pragma once

#include "State.h"

class Server;

// Состояние ожидания: слушает порт и принимает первый кадр входящего
// запроса — это вход в конвейер п.4. Разбор принятого и сборка данных —
// уже ReceivingState (см. цепочку в Server.h).
class IdleState : public State
{
    private:
        Server& server;

    public:
        IdleState(Server& server);

        void enter() override;
        void update() override;
        void exit() override;
        void handleEvent(Event event) override;
};
