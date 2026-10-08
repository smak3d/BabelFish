#include "ErrorState.h"

#include "Server.h"

#include <chrono>
#include <iostream>
#include <thread>

ErrorState::ErrorState(Server& server)
    : server(server)
{
}

void ErrorState::enter()
{
    switch (error)
    {
        case Error::UNKNOWN_ERROR:
            std::cerr << "[server] unknown error\n";
            break;

        case Error::NETWORK_ERROR:
            std::cerr << "[server] network error\n";
            break;

        case Error::TRANSCRIPTION_ERROR:
            // Недостижимо: ошибки моделей уезжают клиенту в Response
            // (см. ProcessingState), цикл они не рушат.
            std::cerr << "[server] transcription error\n";
            break;

        case Error::ASSISTANT_ERROR:
            // Недостижимо — см. TRANSCRIPTION_ERROR выше.
            std::cerr << "[server] assistant error\n";
            break;
    }
}

void ErrorState::update()
{
    // После любой ошибки сокет уже закрыт, а порт слушается заново
    // сразу же — штатное восстановление, а не костыль (проверено пробой
    // в TransportEcho). Если bind не выходит, крутимся с паузой, пока
    // порт не освободится.
    if (!server.ensureListening())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        return;
    }

    server.goIdle();
}

void ErrorState::exit()
{
}

void ErrorState::handleEvent(Event)
{
    // События конвейера раздаёт Server::handleEvent — сюда их никто не шлёт.
}

void ErrorState::setError(Error error)
{
    this->error = error;
}
