#pragma once

#include "MessageType.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace protocol
{

/// Статус, который сервер кладёт в ответ.
///
/// ВАЖНО: это не client::Error и не server::Error. Их числовые значения
/// уже разошлись — у клиента RECORDING_ERROR стоит между NETWORK_ERROR и
/// TRANSCRIPTION_ERROR, у сервера его вообще нет:
///
///     клиент: 0 UNKNOWN 1 RECORDING 2 NETWORK 3 TRANSCRIPTION 4 ASSISTANT
///     сервер: 0 UNKNOWN           1 NETWORK    2 TRANSCRIPTION   3 ASSISTANT
///
/// Отправлять по проводу локальный перечисление нельзя: серверный
/// TRANSCRIPTION_ERROR=2 клиент прочитал бы как NETWORK_ERROR, а
/// ASSISTANT_ERROR=3 — как TRANSCRIPTION_ERROR. По проводу идёт свой
/// перечисление, а маппинг на локальный Error делает получатель.
enum class ResponseStatus : std::uint8_t
{
    Ok                 = 0,
    UnknownError       = 1,
    NetworkError       = 2,
    TranscriptionError = 3,
    AssistantError     = 4,
};

/// Ответ сервера. Строки на проводе — UTF-8.
///
///     [заголовок: 6][status: 1][transcriptionLen: 4][transcription]
///                  [responseLen: 4][response]
struct Response
{
    std::uint32_t   id{};            // копия Request::id
    ResponseStatus  status{};
    std::string     transcription;   // что распознал whisper
    std::string     response;        // что ответил ассистент

    bool operator==(const Response&) const = default;

    bool ok() const noexcept { return status == ResponseStatus::Ok; }

    // --- wire -------------------------------------------------------------

    std::vector<std::byte> serialize() const
    {
        const MessageHeader header{ MessageType::Result, id };
        const auto head = header.serialize();

        std::vector<std::byte> out;
        out.reserve(head.size() + 1 + 8 + transcription.size() + response.size());

        out.insert(out.end(), head.begin(), head.end());

        wire::putU8(out, static_cast<std::uint8_t>(status));
        wire::putString(out, transcription);
        wire::putString(out, response);

        return out;
    }

    static std::optional<Response> deserialize(std::span<const std::byte> in, std::size_t& pos)
    {
        const auto header = MessageHeader::deserialize(in, pos);
        if (!header)
            return std::nullopt;

        // Один ответ сейчас — Result. Появится новый тип ответа с другим
        // layout — сюда придёт отдельный разбор, а не «на всякий случай».
        if (header->type != MessageType::Result)
            return std::nullopt;

        const auto seenStatus = wire::getU8(in, pos);
        if (!seenStatus)
            return std::nullopt;

        auto seenTranscription = wire::getString(in, pos);
        if (!seenTranscription)
            return std::nullopt;

        auto seenResponse = wire::getString(in, pos);
        if (!seenResponse)
            return std::nullopt;

        Response response;
        response.id            = header->id;
        response.status        = static_cast<ResponseStatus>(*seenStatus);
        response.transcription = std::move(*seenTranscription);
        response.response      = std::move(*seenResponse);
        pos = in.size();

        return response;
    }

    static std::optional<Response> deserialize(std::span<const std::byte> in)
    {
        std::size_t pos = 0;
        return deserialize(in, pos);
    }

    // --- ZeroMQ -----------------------------------------------------------

    zmq::message_t toMessage() const
    {
        const auto bytes = serialize();
        return zmq::message_t(bytes.data(), bytes.size());
    }

    static std::optional<Response> fromMessage(const zmq::message_t& message)
    {
        return deserialize(asBytes(message));
    }
};

} // namespace protocol
