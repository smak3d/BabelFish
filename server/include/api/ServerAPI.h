#pragma once

class Server;

// Диспетчер между ReceivingState и ProcessingState: место, где принятый
// запрос превращается в план обработки. Цепочка из todo:
//
//     ReceivingState -> ServerAPI::dispatch -> ProcessingState -> Router
//
// Диспетчер только решает, берёмся ли мы за запрос (Process/Reject).
// Выбор исполнителя — Router (п.8): он живёт в ProcessingState после
// склейки аудио/текста и тоже не меняет ни состояний, ни контракт.
class ServerAPI
{
    public:
        // Куда вести принятый запрос.
        enum class Route
        {
            Process,    // берём в работу: дальше ProcessingState
            Reject      // не берём: отказ уже лежит в server.getResponse(),
                        // уедет клиенту через SendingState
        };

        // Решение по запросу, лежащему в server.getRequest(). Для Reject
        // в server.getResponse() уже записан готовый ответ клиенту.
        Route dispatch(Server& server);
};
