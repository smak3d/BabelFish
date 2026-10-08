#include "WaitingState.h"

#include "BabelFish.h"

#include "Response.h"

#include <iostream>

namespace
{
    // Имя статуса для лога. Отдельная функция нужна потому, что по
    // проводу своё перечисление (Response.h): локальные имена ErrorState
    // не годятся — числовые значения уже разошлись.
    const char* statusName(protocol::ResponseStatus status)
    {
        switch (status)
        {
            case protocol::ResponseStatus::Ok:                 return "ok";
            case protocol::ResponseStatus::UnknownError:       return "unknown error";
            case protocol::ResponseStatus::NetworkError:       return "network error";
            case protocol::ResponseStatus::TranscriptionError: return "transcription error";
            case protocol::ResponseStatus::AssistantError:     return "assistant error";
        }

        return "unknown";
    }

    // Маппинг проводного статуса на локальный Error. Приведением типа
    // быть не может: клиентский Error и ResponseStatus — независимые
    // перечисления с разными значениями (см. комментарий в Response.h).
    Error toLocalError(protocol::ResponseStatus status)
    {
        switch (status)
        {
            // Вызывается только для !ok(), но ветка обязана существовать:
            // switch по перечислению должен быть полным.
            case protocol::ResponseStatus::Ok:
                return Error::UNKNOWN_ERROR;

            case protocol::ResponseStatus::UnknownError:
                return Error::UNKNOWN_ERROR;

            case protocol::ResponseStatus::NetworkError:
                return Error::NETWORK_ERROR;

            case protocol::ResponseStatus::TranscriptionError:
                return Error::TRANSCRIPTION_ERROR;

            case protocol::ResponseStatus::AssistantError:
                return Error::ASSISTANT_ERROR;
        }

        return Error::UNKNOWN_ERROR;
    }
}

WaitingState::WaitingState(
    BabelFish& app
)
    : app(app)
{
}

void WaitingState::enter()
{
    std::cout << "[client] waiting for response (id "
              << app.getRequest().id << ", timeout "
              << app.getNetwork().receiveTimeout() << " ms)\n";
}

void WaitingState::update()
{
    auto frame = app.getNetwork().receive();
    if (!frame)
    {
        // Приём упал: таймаут (сервер молчит дольше receiveTimeout) либо
        // обрыв — сокет уже закрыт, контракт NetworkClient, дальше только
        // connect() заново, его сделает ErrorState. zmq не выдаёт
        // отдельного кода «пир пропал»: под умершим собеседником он прячет
        // таймаут (см. NetworkError.h), поэтому события разные, а ошибка
        // сети одна.
        const auto event =
            app.getNetwork().lastError() == NetworkError::Timeout
                ? Event::NETWORK_TIMEOUT
                : Event::CONNECTION_LOST;

        std::cerr << "[client] receive failed: "
                  << networkErrorName(app.getNetwork().lastError()) << " - "
                  << app.getNetwork().lastErrorText() << "\n";

        app.handleEvent(event, false);
        return;
    }

    const auto response = protocol::Response::fromMessage(*frame);

    // Битый кадр или ответ с чужим id: цикл REQ/REP этим кадром уже
    // завершён, повторный receive() без нового запроса упадёт с EFSM —
    // ждать дальше нельзя, только connect() заново. Провод нам
    // испортился, поэтому ошибка сетевая.
    if (!response || response->id != app.getRequest().id)
    {
        std::cerr << "[client] malformed or foreign response (expected id "
                  << app.getRequest().id << ", got "
                  << (response ? std::to_string(response->id)
                               : std::string("unparsable"))
                  << ")\n";

        app.handleEvent(Event::RESPONSE_RECEIVED, false);
        return;
    }

    app.setTranscription(response->transcription);
    app.setResponse(response->response);

    if (!response->ok())
    {
        // Сервер отработал, но с ошибкой — статус уехал по проводу своим
        // перечислением (Response.h). Это НЕ ошибка сети: соединение
        // живо, но показывать тут нечего — цикл закольцуется через
        // ErrorState, который напечатает имя ошибки.
        std::cerr << "[client] server replied with error: "
                  << statusName(response->status);
        if (!response->response.empty())
            std::cerr << " - " << response->response;
        std::cerr << "\n";

        app.fail(toLocalError(response->status));
        return;
    }

    app.handleEvent(Event::RESPONSE_RECEIVED, true);
}

void WaitingState::exit()
{
}

void WaitingState::handleEvent(Event)
{
    // События конвейера раздаёт BabelFish::handleEvent — сюда их никто не шлёт.
}
