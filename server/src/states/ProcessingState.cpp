#include "ProcessingState.h"

#include "Server.h"
#include "Router.h"

#include <exception>
#include <iostream>

ProcessingState::ProcessingState(Server& server)
    : server(server)
{
}

void ProcessingState::enter()
{
    std::cout << "[server] processing\n";
}

void ProcessingState::update()
{
    auto& response       = server.getResponse();
    const auto& request  = server.getRequest();

    response = protocol::Response{};
    response.id     = request.id;
    response.status = protocol::ResponseStatus::Ok;

    if (request.type == protocol::MessageType::AudioRequest)
    {
        try
        {
            response.transcription =
                server.getTranscriber().transcribe(server.getAudio());
        }
        catch (const std::exception& exception)
        {
            // Модель не загрузилась или whisper упал: клиенту уезжает
            // статус и причина, цикл не рушится — отправит SendingState.
            response.status   = protocol::ResponseStatus::TranscriptionError;
            response.response = exception.what();

            server.handleEvent(Event::PROCESSING_FINISHED, true);
            return;
        }
    }
    else
    {
        // Текстовый запрос: распознавать нечего — присланный текст и есть
        // «распознанное». Кладём его в transcription, чтобы клиент мог
        // показать, с чем работал ассистент.
        response.transcription = server.getText();
    }

    if (response.transcription.empty())
    {
        // Тишина: спрашивать ассистента нечего. Пустая транскрипция с
        // пустым ответом и статусом Ok — не ошибка, а честный результат.
        server.handleEvent(Event::PROCESSING_FINISHED, true);
        return;
    }

    // П.8, связка: запрос -> маршрутизатор -> исполнитель -> LLM.
    // Семантика п.8: исключение роутера запрос не ломает — остаётся
    // None, фолбэк в LocalAssistant (сегодняшнее поведение).
    Router::Executor kind   = Router::Executor::None;
    Router::Params   params;
    try
    {
        const auto [intent, extracted] =
            Router::classify(response.transcription);
        params = extracted;
        kind   = Router::executorFor(intent, params);
    }
    catch (const std::exception&)
    {
        // Остаётся None — фолбэк в LocalAssistant.
    }

    try
    {
        response.response =
            server.getExecutor(kind).execute(response.transcription, params);
    }
    catch (const std::exception& exception)
    {
        // Ошибки исполнителя (модель, сеть, удалённый недоступен) —
        // AssistantError, цикл не рушится.
        response.status   = protocol::ResponseStatus::AssistantError;
        response.response = exception.what();
    }

    server.handleEvent(Event::PROCESSING_FINISHED, true);
}

void ProcessingState::exit()
{
}

void ProcessingState::handleEvent(Event)
{
    // События конвейера раздаёт Server::handleEvent — сюда их никто не шлёт.
}
