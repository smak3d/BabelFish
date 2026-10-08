#include "Server.h"

#include <iostream>
#include <utility>

Server::Server(std::string endpoint)
    : endpoint(std::move(endpoint)),
      errorState(*this),
      idleState(*this),
      receivingState(*this),
      processingState(*this),
      sendingState(*this),
      currentState(&idleState)
{
    // Порт слушается сразу же — enter() IdleState вызывает
    // ensureListening(), поэтому к моменту первого update() сокет уже
    // ждёт кадр (или ошибка listen() уже в логе, её доработает
    // первый update() -> ErrorState).
    currentState->enter();
}

void Server::update()
{
    currentState->update();
}

void Server::run()
{
    // Выход — Ctrl+C: процесс завершается, сокеты и модели отдаёт OS.
    // Граничный shutdown не нужен: receive() и так ждёт кадр бесконечно
    // (так устроен NetworkServer, см. его контракт), и под сервер концепция
    // «живёт, пока жив дом».
    while (true)
        update();
}

void Server::handleEvent(Event event, bool success)
{
    if (!success)
    {
        switch (event)
        {
            case Event::REQUEST_RECEIVED:
            case Event::RESPONSE_SENT:
                // Приём и отправка — это всегда сеть: listen()/receive()/
                // send() упал, сокет уже закрыт (контракт NetworkServer),
                // дальше только listen() заново — его сделает ErrorState.
                errorState.setError(Error::NETWORK_ERROR);
                break;

            case Event::RECEIVING_FINISHED:
            case Event::PROCESSING_FINISHED:
                // Недостижимо: оба состояния всегда шлют success == true —
                // сбой данных уезжает клиенту в Response, а не рушит цикл
                // (REP обязан ответить на каждый приём). Ветка нужна, чтобы
                // switch по перечислению был полным.
                errorState.setError(Error::UNKNOWN_ERROR);
                break;
        }

        changeState(&errorState);
        return;
    }

    switch (event)
    {
        case Event::REQUEST_RECEIVED:
            changeState(&receivingState);
            break;

        case Event::RECEIVING_FINISHED:
            // В обход Router (п.4): маршрутов пока один — диспетчер
            // решает, берёмся ли мы вообще за этот запрос. Router (п.6)
            // навяжется сюда же и добавит выбор net-ассистента, не меняя
            // ни состояний, ни контракт.
            if (api.dispatch(*this) == ServerAPI::Route::Process)
                changeState(&processingState);
            else
                changeState(&sendingState); // готовый отказ ждёт в response
            break;

        case Event::PROCESSING_FINISHED:
            changeState(&sendingState);
            break;

        case Event::RESPONSE_SENT:
            changeState(&idleState);
            break;
    }
}

void Server::changeState(State* newState)
{
    if (currentState)
    {
        currentState->exit();
    }
    currentState = newState;
    if (currentState)
    {
        currentState->enter();
    }
}

State* Server::getCurrentState() const
{
    return currentState;
}

bool Server::isIdle() const
{
    return currentState == &idleState;
}

bool Server::isListening() const
{
    return network.isListening();
}

bool Server::ensureListening()
{
    if (network.isListening())
    {
        // Сокет жив: повторный listen() закрыл бы работающий bind и
        // заодно породил бы лишний цикл close/bind на каждом ответе.
        return true;
    }

    if (!network.listen(endpoint))
    {
        std::cerr << "[server] listen " << endpoint << " failed: "
                  << networkErrorName(network.lastError()) << " - "
                  << network.lastErrorText() << "\n";
        return false;
    }

    std::cout << "[server] listening on " << endpoint << "\n";
    return true;
}

void Server::goIdle()
{
    changeState(&idleState);
}

void Server::replyBrokenFrame(const zmq::message_t& frame)
{
    // id вытаскиваем из заголовка, если он хоть как-то читается: клиент
    // сопоставляет ответ по id, и ответ на мусор с мусорным id — лучшее,
    // что тут можно сделать. Заголовок не читается вовсе — id 0: свой
    // RequestBuilder никогда не выдаёт ноль, чужому клиенту такой ответ
    // просто не сопоставится.
    const auto header =
        protocol::MessageHeader::deserialize(protocol::asBytes(frame));

    response = protocol::Response{};
    response.id        = header ? header->id : 0;
    response.status    = protocol::ResponseStatus::UnknownError;
    response.response  = "malformed request";

    std::cerr << "[server] malformed request, replying with error (id "
              << response.id << ")\n";

    // REP обязан ответить на каждый принятый кадр, иначе следующий
    // receive() упадёт с EFSM. Ответ ушёл — остаёмся ждать дальше;
    // не ушёл — это уже сетевая ошибка, сокет закрыт.
    if (!network.send(response.toMessage()))
        handleEvent(Event::RESPONSE_SENT, false);
}

NetworkServer& Server::getNetwork()
{
    return network;
}

Transcriber& Server::getTranscriber()
{
    if (!transcriber)
        transcriber = std::make_unique<Transcriber>(sampleRate);

    return *transcriber;
}

LocalAssistant& Server::getAssistant()
{
    if (!assistant)
        assistant = std::make_unique<LocalAssistant>();

    return *assistant;
}

protocol::Request& Server::getRequest()
{
    return request;
}

void Server::setRequest(const protocol::Request& request)
{
    this->request = request;
}

protocol::Response& Server::getResponse()
{
    return response;
}

std::vector<std::int16_t>& Server::getAudio()
{
    return audio;
}

void Server::setAudio(const std::vector<std::int16_t>& audio)
{
    this->audio = audio;
}

std::string& Server::getText()
{
    return text;
}

void Server::setText(const std::string& text)
{
    this->text = text;
}
