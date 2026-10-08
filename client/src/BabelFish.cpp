#include "BabelFish.h"

#include <iostream>
#include <utility>

BabelFish::BabelFish(std::string host, std::uint16_t port, InputMode mode)
    : host(std::move(host)),
      port(port),
      mode(mode),
      recorder(sampleRate, channels),
      builder(sampleRate, channels),
      errorState(*this),
      idleState(*this),
      inputState(*this, keyboardInput, builder),
      recordingState(*this, recorder),
      sendingState(*this),
      waitingState(*this),
      showResultState(*this),
      currentState(&idleState)
{
    // Соединение ставится сразу же — enter() IdleState зовёт
    // ensureConnected(): zmq делает connect() асинхронно, до первого
    // send()/receive() это просто подготовка сокета (см. NetworkClient.h).
    currentState->enter();
}

void BabelFish::update()
{
    currentState->update();
}

void BabelFish::handleEvent(Event event, bool success)
{
    // Сетевые события успешного исхода не имеют: таймаут и обрыв — это
    // всегда ошибка сети, каким бы success их ни прислал вызывающий.
    if (event == Event::NETWORK_TIMEOUT || event == Event::CONNECTION_LOST)
    {
        success = false;
    }

    if(!success)
    {
        Error error = Error::UNKNOWN_ERROR;

        switch(event)
        {
            case Event::BUTTON_PRESSED:
            case Event::BUTTON_RELEASED:
            case Event::RECORDING_FINISHED:
                // Сбор данных: пустой ввод (InputState шлёт BUTTON_RELEASED
                // с success == false) и сбой Recorder — это рекордер, а
                // не сеть.
                error = Error::RECORDING_ERROR;
                break;
            case Event::SENDING_FINISHED:
            case Event::RESPONSE_RECEIVED:
            case Event::NETWORK_TIMEOUT:
            case Event::CONNECTION_LOST:
                // Сокет уже закрыт — контракт NetworkClient, дальше только
                // connect() заново, его сделает ErrorState.
                error = Error::NETWORK_ERROR;
                break;
            case Event::RESULT_SHOWN:
                // Недостижимо: ShowResultState всегда шлёт success == true.
                // Ветка нужна, чтобы switch по перечислению был полным.
                error = Error::UNKNOWN_ERROR;
                break;
        }

        fail(error);
        return;
    }

    switch(event)
    {
        case Event::BUTTON_PRESSED:
            // RECORDING: чем собираются данные, решает режим ввода (п.3).
            if (mode == InputMode::Keyboard)
                changeState(&inputState);
            else
                changeState(&recordingState);
            break;
        case Event::BUTTON_RELEASED:
            // InputState собрал готовый запрос (иначе событие ушло бы с
            // success == false): перекладываем его в SendingState.
            request = *inputState.getRequest();
            changeState(&sendingState);
            break;
        case Event::RECORDING_FINISHED:
            // Записанный сигнал есть, упаковки не было (Input не
            // участвовал) — AudioRequest собирает RequestBuilder здесь
            // (п.3: «прямые сборки для случаев, когда Input не участвует»).
            request = builder.buildAudio(audio);
            changeState(&sendingState);
            break;
        case Event::SENDING_FINISHED:
            changeState(&waitingState);
            break;
        case Event::RESPONSE_RECEIVED:
            changeState(&showResultState);
            break;
        case Event::RESULT_SHOWN:
            changeState(&idleState);
            break;
        case Event::NETWORK_TIMEOUT:
        case Event::CONNECTION_LOST:
            // Недостижимо: выше success для этих событий принудительно
            // false. Ветка нужна, чтобы switch по перечислению был полным.
            break;
    }
}

void BabelFish::changeState(State* newState)
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

void BabelFish::fail(Error error)
{
    errorState.setError(error);
    changeState(&errorState);
}

State* BabelFish::getCurrentState()
{
    return currentState;
}

bool BabelFish::isIdle() const
{
    return currentState == &idleState;
}

bool BabelFish::ensureConnected()
{
    if (network.isConnected())
    {
        // Сокет жив: повторный connect() закрыл бы работающее соединение
        // и начал его заново без причины.
        return true;
    }

    if (!network.connect(host, port))
    {
        std::cerr << "[client] connect " << host << ":" << port << " failed: "
                  << networkErrorName(network.lastError()) << " - "
                  << network.lastErrorText() << "\n";
        return false;
    }

    std::cout << "[client] connected to " << host << ":" << port << "\n";
    return true;
}

void BabelFish::goIdle()
{
    changeState(&idleState);
}

NetworkClient& BabelFish::getNetwork()
{
    return network;
}

const protocol::Request& BabelFish::getRequest() const
{
    return request;
}

std::vector<int16_t>&BabelFish::getAudio()
{
    return audio;
}
void BabelFish::setAudio(const std::vector<int16_t>& audio)
{
    this->audio = audio;
}

std::string& BabelFish::getTranscription()
{
    return transcription;
}

void BabelFish::setTranscription(const std::string& text)
{
    this->transcription = text;
}


std::string& BabelFish::getResponse()
{
    return response;
}

void BabelFish::setResponse(const std::string& text)
{
    this->response = text;
}
