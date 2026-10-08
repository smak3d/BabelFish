#include "ServerAPI.h"

#include "Server.h"

#include "MessageType.h"
#include "Response.h"

#include <string>
#include <utility>

namespace
{
    // Отказ не ломает конвейер: ответ кладём в server, а уходит он обычным
    // SendingState — тем же путём, что и успех. REP так и требует: на
    // каждый принятый запрос обязан прийти ответ, иначе следующий приём
    // упадёт с EFSM.
    ServerAPI::Route reject(Server& server,
                            protocol::ResponseStatus status,
                            std::string reason)
    {
        auto& response = server.getResponse();

        response = protocol::Response{};
        response.id        = server.getRequest().id;
        response.status    = status;
        response.response  = std::move(reason);

        return ServerAPI::Route::Reject;
    }
}

ServerAPI::Route ServerAPI::dispatch(Server& server)
{
    const auto& request = server.getRequest();

    switch (request.type)
    {
        case protocol::MessageType::AudioRequest:
            if (server.getAudio().empty())
            {
                return reject(server,
                              protocol::ResponseStatus::UnknownError,
                              "empty or malformed audio payload");
            }

            // Формат едет с запросом (п.1): расхождение с тем, что умеет
            // Transcriber, — это ошибка, а не тишина. Статус
            // TranscriptionError, потому что причина в аудио.
            if (request.audioFormat.sampleRate != Server::sampleRate ||
                request.audioFormat.channels    != Server::channels)
            {
                return reject(server,
                              protocol::ResponseStatus::TranscriptionError,
                              "unsupported audio format: expected "
                                  + std::to_string(Server::sampleRate)
                                  + " Hz/" + std::to_string(Server::channels)
                                  + " ch, got "
                                  + std::to_string(request.audioFormat.sampleRate)
                                  + " Hz/"
                                  + std::to_string(request.audioFormat.channels)
                                  + " ch");
            }

            return Route::Process;

        case protocol::MessageType::TextRequest:
            if (server.getText().empty())
            {
                return reject(server,
                              protocol::ResponseStatus::UnknownError,
                              "empty text payload");
            }

            return Route::Process;

        default:
            // ImageRequest и RawRequest — п.6/п.7: другие модели и чанки.
            return reject(server,
                          protocol::ResponseStatus::UnknownError,
                          "unsupported request type");
    }
}
