#include "ReceivingState.h"

#include "Server.h"

#include <iostream>

ReceivingState::ReceivingState(Server& server)
    : server(server)
{
}

void ReceivingState::enter()
{
    std::cout << "[server] receiving request\n";
}

void ReceivingState::update()
{
    const auto& request = server.getRequest();

    switch (request.type)
    {
        case protocol::MessageType::AudioRequest:
        {
            // nullopt = payload режется посередине сэмпла (повреждённый
            // кадр): собираем пусто, откажет диспетчер — так данные не
            // доедут до Transcriber.
            const auto samples = request.getAudio();
            server.setAudio(samples ? *samples : std::vector<std::int16_t>{});
            server.setText("");
            break;
        }

        case protocol::MessageType::TextRequest:
            // UTF-8: клиентское закодированное в байты, здесь обратно.
            server.setText(std::string(
                reinterpret_cast<const char*>(request.payload.data()),
                request.payload.size()));
            server.setAudio({});
            break;

        default:
            // Неподдерживаемый тип данных не собираем — откажет
            // диспетчер (ServerAPI), ответ уедет обычным SendingState.
            server.setAudio({});
            server.setText("");
            break;
    }

    server.handleEvent(Event::RECEIVING_FINISHED, true);
}

void ReceivingState::exit()
{
}

void ReceivingState::handleEvent(Event)
{
    // События конвейера раздаёт Server::handleEvent — сюда их никто не шлёт.
}
