#include "ErrorState.h"

#include "BabelFish.h"

#include <chrono>
#include <iostream>
#include <thread>

ErrorState::ErrorState(BabelFish& app)
    : app(app)
{
}

void ErrorState::enter()
{
    switch (error)
    {
        case Error::UNKNOWN_ERROR:
            std::cerr << "[client] unknown error\n";
            break;

        case Error::RECORDING_ERROR:
            std::cerr << "[client] recording error\n";
            break;

        case Error::NETWORK_ERROR:
            std::cerr << "[client] network error\n";
            break;

        case Error::TRANSCRIPTION_ERROR:
            // Причина — от сервера, она лежит в getResponse() и её уже
            // напечатал WaitingState, здесь только имя ошибки.
            std::cerr << "[client] transcription error\n";
            break;

        case Error::ASSISTANT_ERROR:
            // См. TRANSCRIPTION_ERROR выше.
            std::cerr << "[client] assistant error\n";
            break;
    }
}

void ErrorState::update()
{
    // После сетевой ошибки сокет уже закрыт, а переподключение — штатное
    // восстановление, а не костыль: zmq ставит connect() асинхронно.
    // Если соединение поставить не вышло (сокетный ад в zmq — на практике
    // почти недостижимо), крутимся с паузой, как серверный ErrorState
    // при занятом порту.
    if (!app.ensureConnected())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        return;
    }

    app.goIdle();
}

void ErrorState::exit()
{
}

void ErrorState::handleEvent(Event)
{
    // События конвейера раздаёт BabelFish::handleEvent — сюда их никто не шлёт.
}

void ErrorState::setError(Error error)
{
    this->error = error;
}
