#pragma once

#include "State.h"
#include "Event.h"
#include "Error.h"

#include "ServerAPI.h"
#include "NetworkServer.h"
#include "Transcriber.h"
#include "LocalAssistant.h"

#include "ErrorState.h"
#include "IdleState.h"
#include "ReceivingState.h"
#include "ProcessingState.h"
#include "SendingState.h"

#include "Request.h"
#include "Response.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Сервер — то же, чем для клиента является BabelFish: контейнер состояний,
// сервисов и данных одного запроса. Цепочка п.4:
//
//     IdleState        слушает порт, принимает первый кадр
//         --REQUEST_RECEIVED-->
//     ReceivingState   разбирает принятый Request, собирает аудио/текст
//         --RECEIVING_FINISHED-->   диспетчер ServerAPI::dispatch
//     ProcessingState  Transcriber -> LocalAssistant
//         --PROCESSING_FINISHED-->
//     SendingState     Response клиенту
//         --RESPONSE_SENT--> снова IdleState
//
// Любая ошибка сети ведёт в ErrorState: после неё сокет уже закрыт
// (контракт NetworkServer), ErrorState поднимает listen() заново и
// возвращает конвейер в строй — порт слушается сразу же, это проверено
// пробой в TransportEcho.
//
// Ошибки транскрибирования и ассистента цикл НЕ рушат: они уезжают
// клиенту в Response (ResponseStatus). По проводу идёт своё
// перечисление, а маппинг на локальный Error делает получатель — см.
// комментарий в Response.h.
class Server
{
    public:
        // Формат, которым сервер готов заниматься. Тот же, что у Recorder
        // клиента (16000, 1): формат едет с запросом (п.1), поэтому
        // расхождение становится ошибкой ответа, а не тишиной —
        // см. AudioFormat.h.
        static constexpr std::uint32_t sampleRate = 16000;
        static constexpr std::uint16_t channels   = 1;

        // endpoint — куда слушать: "tcp://*:5555" в работе,
        // "tcp://127.0.0.1:<port>" в тестах (общедоступный bind тестам
        // не нужен, он дёргал бы брандмауэр).
        Server(std::string endpoint);

        void update();
        void run(); // главный цикл, не возвращается

        void handleEvent(Event event, bool success);
        void changeState(State* newState);

        State* getCurrentState() const;
        bool isIdle() const;
        bool isListening() const;

        // Штатное восстановление после ErrorState: listen() при
        // необходимости, иначе просто проверка, что сокет жив.
        bool ensureListening();
        void goIdle(); // ErrorState -> IdleState

        // Ответ на битый кадр: REP обязан ответить на каждый приём,
        // иначе следующий receive() упадёт с EFSM.
        void replyBrokenFrame(const zmq::message_t& frame);

        NetworkServer& getNetwork();

        // Ленивая загрузка моделей: см. поле transcriber внизу.
        // Бросают исключение, если файла модели нет — ProcessingState
        // превращает его в Response(TranscriptionError/AssistantError).
        Transcriber& getTranscriber();
        LocalAssistant& getAssistant();

        protocol::Request& getRequest();
        void setRequest(const protocol::Request& request);

        // Ответ клиенту: его целиком готовит ProcessingState или
        // диспетчер при отказе, а отправляет SendingState.
        protocol::Response& getResponse();

        std::vector<std::int16_t>& getAudio();
        void setAudio(const std::vector<std::int16_t>& audio);
        std::string& getText();
        void setText(const std::string& text);

    private:
        std::string endpoint;

        NetworkServer network;
        ServerAPI api;

        // Лениво: модели весят полтора и три гигабайта — грузить их в
        // конструкторе значит не подниматься вовсе на машине без моделей.
        // Первый же запрос, которому нужна модель, догрузит её; если
        // файла нет — исключение уймёт ProcessingState в Response.
        std::unique_ptr<Transcriber> transcriber;
        std::unique_ptr<LocalAssistant> assistant;

        ErrorState errorState;
        IdleState idleState;
        ReceivingState receivingState;
        ProcessingState processingState;
        SendingState sendingState;

        State* currentState;

        // Данные текущего запроса: приняли -> собрали -> обработали ->
        // ответили. Полный сброс на каждом запросе делает dispatch и
        // ProcessingState (см. ServerAPI.cpp и ProcessingState.cpp).
        protocol::Request request;
        protocol::Response response;
        std::vector<std::int16_t> audio;
        std::string text;
};
