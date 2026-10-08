#pragma once

#include "State.h"

class Server;

// Состояние ответа: Response (его целиком уже собрали ProcessingState
// или диспетчер при отказе) уходит клиенту. После удачной отправки
// конвейер возвращается в IdleState и ждёт следующий запрос — тот же
// сокет, без пересоздания.
class SendingState : public State
{
    private:
        Server& server;

    public:
        SendingState(Server& server);

        void enter() override;
        void update() override;
        void exit() override;
        void handleEvent(Event event) override;
};
