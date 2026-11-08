#include "Server.h"

#include <chrono>
#include <iostream>
#include <string>
#include <utility>

namespace
{
    std::string deviceName(int device)
    {
        return device >= 0 ? "gpu " + std::to_string(device) : "cpu";
    }

    // Загрузка модели с замером времени: на больших моделях это десятки
    // секунд, и в логе должно быть видно, что именно и сколько грузилось.
    // std::endl не просто так: stdout сервера часто заворачивают в файл,
    // а при буферизации вывод теряется при любом сбое.
    void logModel(const char* what, const std::string& path, int device,
                  std::chrono::steady_clock::time_point started)
    {
        const auto seconds =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - started).count() / 1000.0;

        std::cout << "[server] " << what << ": " << path
                  << " on " << deviceName(device)
                  << " (" << seconds << " s)" << std::endl;
    }
}

Server::Server(std::string endpoint, ModelSettings settings)
    : endpoint(std::move(endpoint)),
      settings(std::move(settings)),
      promptFn([this](const std::string& system, const std::string& text)
               {
                   // Ленивая модель: getAssistant() грузит её при первом
                   // же вызове, здесь только проброска.
                   return getAssistant().prompt(system, text);
               }),
      localExecutor(promptFn),
      translatorExecutor(promptFn),
      remoteExecutor(),
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
            // Диспетчер только решает, берёмся ли мы за этот запрос;
            // выбор исполнителя — Router (п.8) в ProcessingState, он тоже
            // не меняет ни состояний, ни контракт.
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

    std::cout << "[server] listening on " << endpoint << std::endl;
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
    {
        transcriber = std::make_unique<Transcriber>(
            sampleRate,
            settings.transcriberModel,
            settings.transcriberDevice);
    }

    return *transcriber;
}

LocalAssistant& Server::getAssistant()
{
    if (!assistant)
    {
        assistant = std::make_unique<LocalAssistant>(
            settings.assistantModel,
            settings.assistantDevice,
            settings.assistantLayers);
    }

    return *assistant;
}

Executor& Server::getExecutor(Router::Executor kind)
{
    switch (kind)
    {
        case Router::Executor::None: // фолбэк (п.8)
        case Router::Executor::LocalAssistant:
            return localExecutor;

        case Router::Executor::Translator:
            return translatorExecutor;

        case Router::Executor::RemoteTranslator:
            return remoteExecutor;
    }

    // Недостижимо: switch по всем значениям enum. Нужен, чтобы
    // управление не убежало из не-void функции.
    return localExecutor;
}

void Server::preloadModels()
{
    // Грузим обе сразу: на большом железе это десятки секунд, и бросать
    // их в первый запрос — значит заставить клиента ждать. Порядок
    // как в get*: сначала транскрибер, потом ассистент.
    auto started = std::chrono::steady_clock::now();
    getTranscriber();
    logModel("transcriber", settings.transcriberModel,
             settings.transcriberDevice, started);

    started = std::chrono::steady_clock::now();
    getAssistant();
    logModel("assistant", settings.assistantModel,
             settings.assistantDevice, started);
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
