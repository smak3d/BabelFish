#pragma once

#include "State.h"

class BabelFish;

// Состояние отправки: готовый запрос (п.1, собран либо InputState, либо
// handleEvent(RECORDING_FINISHED) — см. BabelFish.h) уходит серверу одним
// кадром: send <- Request.toMessage (п.5). После удачного send() REQ ждёт
// ответ — конвейер уходит в WaitingState.
class SendingState : public State
{
    private:
        BabelFish& app;
    public:
        SendingState(BabelFish& app);

        void enter() override;
        void update() override;
        void exit() override;
        void handleEvent(Event event)override;
};
