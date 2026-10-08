#pragma once

#include "State.h"
#include "Event.h"
#include "Error.h"

#include "Input.h"
#include "KeyboardInput.h"
#include "Recorder.h"
#include "RequestBuilder.h"
#include "NetworkClient.h"

#include "Request.h"

#include "ErrorState.h"
#include "IdleState.h"
#include "InputState.h"
#include "RecordingState.h"
#include "SendingState.h"
#include "WaitingState.h"
#include "ShowResultState.h"

#include <cstdint>
#include <string>
#include <vector>

// Клиент — то, чем для сервера является Server: контейнер состояний,
// сервисов и данных одного запроса. Цепочка п.5:
//
//     IdleState        соединение на месте, main.cpp стартует цикл
//         --BUTTON_PRESSED-->
//     RECORDING        клавиатура -> InputState (KeyboardInput -> TextRequest)
//                      микрофон  -> RecordingState (Recorder -> PCM16)
//         --BUTTON_RELEASED | RECORDING_FINISHED-->
//     SendingState     Request серверу
//         --SENDING_FINISHED-->
//     WaitingState     Response от сервера
//         --RESPONSE_RECEIVED-->
//     ShowResultState  печать транскрипции и ответа
//         --RESULT_SHOWN--> снова IdleState
//
// Чем собираются данные, решает режим ввода (п.3): клавиатура копит текст
// сама (два Enter — начало и конец), поэтому InputState уносит в
// SendingState готовый запрос через getRequest(); микрофон пишет «по
// событиям» — старт/стоп отдаёт main.cpp, а AudioRequest из записанного
// собирает handleEvent(RECORDING_FINISHED) через RequestBuilder::buildAudio.
//
// Любая ошибка ведёт в ErrorState: после сетевой ошибки сокет уже закрыт
// (контракт NetworkClient), ErrorState ставит connect() заново и цикл
// возвращается в IdleState. Ошибки моделей приходят готовым статусом из
// Response — их маппит WaitingState (по проводу своё перечисление, см.
// Response.h): соединение при них живо, но цикл всё равно закольцовывается.
class BabelFish
{
    public:
        // Формат, которым клиент пишет и отправляет — тот же, что ждёт
        // сервер (Server::sampleRate/channels): формат едет с запросом
        // (п.1), расхождение стало бы ошибкой ответа, а не тишиной.
        static constexpr std::uint32_t sampleRate = 16000;
        static constexpr std::uint16_t channels   = 1;

        // host/port — куда ставить соединение (tcp://<host>:<port>),
        // mode — чем собираются данные: микрофон -> RecordingState,
        // клавиатура -> InputState.
        BabelFish(std::string host, std::uint16_t port, InputMode mode);

        void update();
        void handleEvent(Event event, bool success);
        void changeState(State* newState);

        // Переход в ErrorState с готовой причиной: handleEvent выводит
        // ошибку из вида события, здесь причина приходит извне — ею
        // пользуется WaitingState, когда статус Response (Response.h)
        // превращается в локальный Error.
        void fail(Error error);

        State* getCurrentState();
        bool isIdle() const;

        // Штатное восстановление после ErrorState: connect() при
        // необходимости, иначе просто проверка, что сокет жив.
        bool ensureConnected();
        void goIdle(); // ErrorState -> IdleState

        NetworkClient& getNetwork();

        // Запрос, который уходит или ушёл SendingState'ом: по его id
        // WaitingState сопоставляет пришедший Response.
        const protocol::Request& getRequest() const;

        std::vector<int16_t>& getAudio();
        void setAudio(const std::vector<int16_t>& audio);
        std::string& getTranscription();
        void setTranscription(const std::string& text);
        std::string& getResponse();
        void setResponse(const std::string& text);

    private:
        std::string host;
        std::uint16_t port;
        InputMode mode;

        NetworkClient network;
        Recorder recorder;
        KeyboardInput keyboardInput;
        RequestBuilder builder;

        ErrorState errorState;
        IdleState idleState;
        InputState inputState;
        RecordingState recordingState;
        SendingState sendingState;
        WaitingState waitingState;
        ShowResultState showResultState;

        State* currentState;

        // Данные текущего цикла: собрали -> отправили -> ответили.
        // Полный сброс на каждом цикле делает handleEvent: request
        // перезаписывается на каждом уходе в SendingState, остальное —
        // RecordingState и WaitingState.
        protocol::Request request;
        std::vector<int16_t> audio;
        std::string transcription;
        std::string response;
};
