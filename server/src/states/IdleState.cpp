#include "IdleState.h"

#include "Server.h"

#include <iostream>

IdleState::IdleState(Server& server)
    : server(server)
{
}

void IdleState::enter()
{
    // Порт слушается «сразу же»: на старте и после каждого ответа.
    // Обычно сокет уже жив, и ensureListening() — дешёвая проверка
    // isListening(); после ошибки здесь случается настоящее listen().
    server.ensureListening();
}

void IdleState::update()
{
    // Повторная проверка на случай, если listen() в enter() не вышел:
    // без сокета receive() вернёт NotConnected, и мы пошли бы в
    // ErrorState там, где проще сразу попробовать снова.
    if (!server.ensureListening())
    {
        server.handleEvent(Event::REQUEST_RECEIVED, false);
        return;
    }

    auto frame = server.getNetwork().receive();
    if (!frame)
    {
        // Приём упал (таймаут или обрыв): сокет уже закрыт — контракт
        // NetworkServer, дальше только listen() заново, его сделает
        // ErrorState.
        std::cerr << "[server] receive failed: "
                  << networkErrorName(server.getNetwork().lastError())
                  << " - " << server.getNetwork().lastErrorText() << "\n";

        server.handleEvent(Event::REQUEST_RECEIVED, false);
        return;
    }

    const auto request = protocol::Request::fromMessage(*frame);
    if (!request)
    {
        // Битый кадр: отвечаем ошибкой (REP обязан ответить на каждый
        // приём, иначе следующий receive() упадёт с EFSM) и остаёмся
        // ждать — сокет цел, обработка продолжается.
        server.replyBrokenFrame(*frame);
        return;
    }

    server.setRequest(*request);
    server.handleEvent(Event::REQUEST_RECEIVED, true);
}

void IdleState::exit()
{
}

void IdleState::handleEvent(Event)
{
    // События конвейера раздаёт Server::handleEvent — сюда их никто не шлёт.
}
