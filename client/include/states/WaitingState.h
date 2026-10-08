#pragma once

#include "State.h"

class BabelFish;

// Состояние ожидания: ответ сервера на отправленный запрос — receive ->
// Response::fromMessage (п.5). Сюда же приходят таймаут и обрыв: события
// NETWORK_TIMEOUT / CONNECTION_LOST никто не шлёт кроме этого состояния
// (обработчик в BabelFish.cpp). Таймаут по умолчанию 60 с — см.
// NetworkClient::receiveTimeout.
//
// Статус Response — своё перечисление по проводу (Response.h): здесь он
// превращается в локальный Error через BabelFish::fail. Ошибки моделей
// цикл не рушат — соединение живо, ErrorState после печати причины
// возвращает цикл в IdleState.
class WaitingState : public State
{
    private:
        BabelFish& app;
    public:
        WaitingState(BabelFish& app);

        void enter() override;
        void update() override;
        void exit() override;
        void handleEvent(Event event)override;
};
