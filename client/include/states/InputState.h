#pragma once

#include "Request.h"
#include "State.h"

#include <optional>

class BabelFish;
class Input;
class RequestBuilder;

// Состояние сбора ввода: опрашивает выбранный Input (п.3) и, когда данные
// готовы, превращает их в protocol::Request через RequestBuilder.
//
// Какой Input подключён — решает тот, кто создаёт состояние (п.5):
// клавиатура -> текст -> TextRequest, микрофон -> запись -> AudioRequest.
// Состояние об этом не знает: оно работает только с интерфейсом Input.
//
// Готовый запрос отдаёт наружу через getRequest() — дальше его уносит
// SendingState (п.5). Ошибки ввода (пустые данные) уходят в ErrorState
// событием BUTTON_RELEASED с success == false.
class InputState : public State
{
    private:
        BabelFish& app;
        Input& input;
        RequestBuilder& builder;

        // Последний собранный запрос. nullopt = данных ещё нет
        // или они вышли пустыми.
        std::optional<protocol::Request> request;

    public:
        // input и builder обязаны жить дольше состояния: состояние
        // только ссылается на них (как RecordingState на Recorder).
        InputState(BabelFish& app, Input& input, RequestBuilder& builder);

        void enter() override;
        void update() override;
        void exit() override;
        void handleEvent(Event event) override;

        // Готовый байтовый запрос (п.1) или nullopt.
        const std::optional<protocol::Request>& getRequest() const;
};
