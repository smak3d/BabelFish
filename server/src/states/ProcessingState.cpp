#include "ProcessingState.h"

#include "Server.h"

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

    try
    {
        response.response =
            server.getAssistant().prompt(response.transcription);
    }
    catch (const std::exception& exception)
    {
        response.status = protocol::ResponseStatus::AssistantError;
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
