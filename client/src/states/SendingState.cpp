#include "SendingState.h"

#include "BabelFish.h"

#include <iostream>

SendingState::SendingState(
    BabelFish& app
)
    : app(app)
{
}

void SendingState::enter()
{
    std::cout << "[client] sending request (id "
              << app.getRequest().id << ")\n";
}

void SendingState::update()
{
    // Request.toMessage() — байты на проводе. Ошибка отправки (сокет не
    // подключён, таймаут, EFSM) закрывает соединение — контракт
    // NetworkClient, дальше только connect() заново, его сделает ErrorState.
    if (!app.getNetwork().send(app.getRequest().toMessage()))
    {
        app.handleEvent(Event::SENDING_FINISHED, false);
        return;
    }

    app.handleEvent(Event::SENDING_FINISHED, true);
}

void SendingState::exit()
{
}

void SendingState::handleEvent(Event)
{
    // События конвейера раздаёт BabelFish::handleEvent — сюда их никто не шлёт.
}
