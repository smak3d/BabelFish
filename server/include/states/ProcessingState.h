#pragma once

#include "State.h"

class Server;

// Состояние обработки: Transcriber -> LocalAssistant (оба — готовые
// сервисы, см. services/). Результат складывается в Response целиком,
// включая статус ошибки.
//
// Сбои модели цикл НЕ рушат: исключение превращается в Response со
// статусом TranscriptionError/AssistantError, и клиент всё равно получает
// ответ — REP обязан ответить на каждый приём, а по проводу своё
// перечисление ошибок (Response.h). Поэтому событие всегда уходит с
// success == true: обработка завершилась, как бы она ни закончилась.
class ProcessingState : public State
{
    private:
        Server& server;

    public:
        ProcessingState(Server& server);

        void enter() override;
        void update() override;
        void exit() override;
        void handleEvent(Event event) override;
};
