#pragma once

#include "AudioFormat.h"
#include "MessageType.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <vector>

namespace protocol
{

/// Конверт запроса.
///
///     [заголовок: 6][audioFormat: 6][payload ...]
///
/// `payload` непрозрачен — его разбирает владелец конкретного MessageType:
///   AudioRequest : PCM-сэмплы int16 LE, частота/каналы в `audioFormat`
///   TextRequest  : UTF-8
///   ImageRequest : байты изображения
///   RawRequest   : что угодно под другую модель
///
/// Новый тип запроса = новое значение MessageType, каркас не меняется.
/// Отказ от записи аудио = выпадение `audioFormat`/`getAudio()`, каркас
/// тоже не меняется.
///
/// Для НЕ-аудио-запросов `audioFormat` остаётся нулевым и ни на что не
/// влияет: поле вынесено отдельно, потому что оно задаёт декодирование
/// байтов payload, а не их содержимое.
struct Request
{
    std::uint32_t  id{};            // какой ответ считать ответом на этот запрос
    MessageType    type{};
    AudioFormat    audioFormat{};
    std::vector<std::byte> payload{};

    bool operator==(const Request&) const = default;

    // --- аудио: клиент кладёт int16, сервер достаёт int16 -----------------

    void setAudio(const AudioFormat& format, std::span<const std::int16_t> samples)
    {
        type        = MessageType::AudioRequest;
        audioFormat = format;

        payload.resize(samples.size_bytes());
        if (!samples.empty())
            std::memcpy(payload.data(), samples.data(), samples.size_bytes());
    }

    /// nullopt, если это не аудио-запрос или payload режется посередине
    /// сэмпла (нечётное число байт = повреждённый кадр).
    std::optional<std::vector<std::int16_t>> getAudio() const
    {
        if (type != MessageType::AudioRequest)
            return std::nullopt;

        if (payload.size() % sizeof(std::int16_t) != 0)
            return std::nullopt;

        std::vector<std::int16_t> samples(payload.size() / sizeof(std::int16_t));
        if (!samples.empty())
            std::memcpy(samples.data(), payload.data(), payload.size());

        return samples;
    }

    // --- wire -------------------------------------------------------------

    std::vector<std::byte> serialize() const
    {
        const MessageHeader header{ type, id };

        const auto head   = header.serialize();
        const auto format = audioFormat.serialize();

        std::vector<std::byte> out;
        out.reserve(head.size() + format.size() + payload.size());

        out.insert(out.end(), head.begin(),   head.end());
        out.insert(out.end(), format.begin(), format.end());
        out.insert(out.end(), payload.begin(), payload.end());

        return out;
    }

    static std::optional<Request> deserialize(std::span<const std::byte> in, std::size_t& pos)
    {
        const auto header = MessageHeader::deserialize(in, pos);
        if (!header)
            return std::nullopt;

        if (!isRequest(header->type))
            return std::nullopt;

        const auto format = AudioFormat::deserialize(in, pos);
        if (!format)
            return std::nullopt;

        Request request;
        request.id         = header->id;
        request.type       = header->type;
        request.audioFormat = *format;
        request.payload.assign(in.begin() + static_cast<std::ptrdiff_t>(pos), in.end());
        pos = in.size();

        return request;
    }

    static std::optional<Request> deserialize(std::span<const std::byte> in)
    {
        std::size_t pos = 0;
        return deserialize(in, pos);
    }

    // --- ZeroMQ -----------------------------------------------------------

    /// Отдельное сообщение: [заголовок][audioFormat][payload].
    /// Длину zmq знает сам, поэтому свой префикс длины не нужен.
    zmq::message_t toMessage() const
    {
        const auto bytes = serialize();
        return zmq::message_t(bytes.data(), bytes.size());
    }

    static std::optional<Request> fromMessage(const zmq::message_t& message)
    {
        return deserialize(asBytes(message));
    }
};

} // namespace protocol
