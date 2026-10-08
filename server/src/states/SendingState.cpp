#include "SendingState.h"

#include "Server.h"

#include <iostream>

SendingState::SendingState(Server& server)
    : server(server)
{
}

void SendingState::enter()
{
    std::cout << "[server] sending response\n";
}

void SendingState::update()
{
    // Response.toMessage() — байты на проводе; после send() REP снова
    // может принимать, и конвейер уходит в IdleState ждать следующий
    // запрос.
    if (!server.getNetwork().send(server.getResponse().toMessage()))
    {
        // Ошибка отправки: сокет уже закрыт (контракт NetworkServer),
        // клиент либо получил ответ раньше, либо дождётся своего таймаута.
        server.handleEvent(Event::RESPONSE_SENT, false);
        return;
    }

    server.handleEvent(Event::RESPONSE_SENT, true);
}

void SendingState::exit()
{
}

void SendingState::handleEvent(Event)
{
    // События конвейера раздаёт Server::handleEvent — сюда их никто не шлёт.
}
