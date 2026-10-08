#pragma once

class Server;

// Диспетчер между ReceivingState и ProcessingState: место, где принятый
// запрос превращается в план обработки. Цепочка из todo п.4:
//
//     ReceivingState -> Router/ServerAPI (диспетчер) -> ProcessingState
//
// На первом проходе (п.4) он пишется в обход Router: маршрутов ровно
// один — местный конвейер Transcriber -> LocalAssistant. Router (п.6)
// навяжется здесь же и добавит выбор net-ассистента (JEV/LAYA), не меняя
// ни состояний, ни контракт.
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
