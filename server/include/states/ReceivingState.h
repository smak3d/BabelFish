#pragma once

#include "State.h"

class Server;

// Состояние приёма и сборки: разбирает Request, уже принятый в
// IdleState, и собирает из него рабочие данные — сэмплы для Transcriber
// либо текст для ассистента.
//
// В п.4 запрос приходит одним кадром. Многокадровый приём (чанки, п.7)
// ляжет сюда же: следующие кадры будут приходить отдельными
// REP-обменами, и собираться они будут в тех же getAudio()/getText().
class ReceivingState : public State
{
    private:
        Server& server;

    public:
        ReceivingState(Server& server);

        void enter() override;
        void update() override;
        void exit() override;
        void handleEvent(Event event) override;
};
