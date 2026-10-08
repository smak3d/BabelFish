#pragma once

#include "State.h"
#include "Error.h"

class Server;

// Состояние восстановления: после любой ошибки сокет уже закрыт
// (контракт NetworkServer), поэтому ErrorState поднимает listen() заново
// на том же endpoint и возвращает конвейер в IdleState. Порт слушается
// сразу же — это проверено пробой в TransportEcho.
//
// Если bind не выходит (порт занят чужим процессом), состояние крутится
// с паузой и пробует снова: сервер переживает чужую захватку порта.
class ErrorState : public State
{
    private:
        Server& server;
        Error error = Error::UNKNOWN_ERROR;

    public:
        ErrorState(Server& server);

        void enter() override;
        void update() override;
        void exit() override;
        void handleEvent(Event event) override;

        void setError(Error error);
};
