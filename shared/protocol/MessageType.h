#pragma once

// База wire-формата протокола:
//   - версия протокола и перечень типов сообщений;
//   - заголовок кадра, общий для запросов и ответов;
//   - примитивы записи/чтения целых чисел в little-endian.
//
// Общий заголовок (6 байт):
//   [0]     version : u8
//   [1]         type : u8   = MessageType
//   [2..5]        id : u32  (little-endian)
//
// Отдельной длины кадра в формате НЕТ: длину сообщения знает сам ZeroMQ,
// поэтому тело кадра — это просто остаток буфера после известного префикса.
// Это то, что выигрывает выбор zmq по сравнению с TCP-рамкой [len][...].
//
// Примитивы wire живут здесь же, чтобы не плодить отдельный заголовок:
// ими пользуются AudioFormat.h, Request.h и Response.h.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <zmq.hpp>

namespace protocol
{

/// Версия формата. Меняется — когда меняется layout кадра; принимающая
/// сторона по этому байту отвергает кадры от устаревшей сборки, а не молча
/// интерпретирует их как мусор.
inline constexpr std::uint8_t version = 1;

/// Размер общего заголовка в байтах.
inline constexpr std::size_t headerSize = 6;

enum class MessageType : std::uint8_t
{
    // --- запросы: 0..127 ---
    AudioRequest = 1,   // payload: PCM int16 LE, формат задаёт Request::audioFormat
    TextRequest  = 2,   // payload: UTF-8
    ImageRequest = 3,   // payload: байты изображения
    RawRequest   = 4,   // payload: сырые данные под любую другую модель

    // --- ответы: 128..255 ---
    Result       = 128, // транскрипция и/или ответ ассистента
};

/// Значения < 128 — запрос, >= 128 — ответ. По этому биту принимающая
/// сторона понимает, каким layout разбирать кадр, ещё до чтения тела.
inline constexpr bool isRequest(MessageType type) noexcept
{
    return static_cast<std::uint8_t>(type) < 128;
}

inline constexpr bool isResponse(MessageType type) noexcept
{
    return !isRequest(type);
}

/// Примитивы записи/чтения. Чтение возвращает nullopt, если буфер
/// обрывается на середине поля — битый или обрезанный кадр не должен
/// превращаться в поле со случайным значением.
namespace wire
{
    inline void putU8(std::vector<std::byte>& out, std::uint8_t value)
    {
        out.push_back(static_cast<std::byte>(value));
    }

    inline void putU16(std::vector<std::byte>& out, std::uint16_t value)
    {
        out.push_back(static_cast<std::byte>( value        & 0xFFu));
        out.push_back(static_cast<std::byte>((value >> 8)  & 0xFFu));
    }

    inline void putU32(std::vector<std::byte>& out, std::uint32_t value)
    {
        out.push_back(static_cast<std::byte>( value         & 0xFFu));
        out.push_back(static_cast<std::byte>((value >>  8)  & 0xFFu));
        out.push_back(static_cast<std::byte>((value >> 16)  & 0xFFu));
        out.push_back(static_cast<std::byte>((value >> 24)  & 0xFFu));
    }

    inline std::optional<std::uint8_t> getU8(std::span<const std::byte> in, std::size_t& pos)
    {
        if (in.size() < pos + 1)
            return std::nullopt;

        return static_cast<std::uint8_t>(in[pos++]);
    }

    inline std::optional<std::uint16_t> getU16(std::span<const std::byte> in, std::size_t& pos)
    {
        if (in.size() < pos + 2)
            return std::nullopt;

        const auto low  = static_cast<std::uint16_t>(static_cast<std::uint8_t>(in[pos]));
        const auto high = static_cast<std::uint16_t>(static_cast<std::uint8_t>(in[pos + 1]));
        pos += 2;

        return static_cast<std::uint16_t>(low | (high << 8));
    }

    inline std::optional<std::uint32_t> getU32(std::span<const std::byte> in, std::size_t& pos)
    {
        if (in.size() < pos + 4)
            return std::nullopt;

        const auto b0 = static_cast<std::uint32_t>(static_cast<std::uint8_t>(in[pos]));
        const auto b1 = static_cast<std::uint32_t>(static_cast<std::uint8_t>(in[pos + 1]));
        const auto b2 = static_cast<std::uint32_t>(static_cast<std::uint8_t>(in[pos + 2]));
        const auto b3 = static_cast<std::uint32_t>(static_cast<std::uint8_t>(in[pos + 3]));
        pos += 4;

        return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
    }

    /// Длина строки как u32, затем сами байты. Длина обязательна: после неё
    /// в кадре может идти ещё одно поле, а zmq отдаёт только общую длину.
    inline void putString(std::vector<std::byte>& out, const std::string& value)
    {
        putU32(out, static_cast<std::uint32_t>(value.size()));

        const auto* begin = reinterpret_cast<const std::byte*>(value.data());
        out.insert(out.end(), begin, begin + value.size());
    }

    inline std::optional<std::string> getString(std::span<const std::byte> in, std::size_t& pos)
    {
        const auto length = getU32(in, pos);
        if (!length)
            return std::nullopt;

        // pos <= in.size(), length <= 4 ГиБ — переполнения size_t не будет,
        // но само условие ловит завышенную длину в повреждённом кадре.
        if (in.size() < pos + static_cast<std::size_t>(*length))
            return std::nullopt;

        const auto* begin = reinterpret_cast<const char*>(in.data() + pos);
        pos += *length;

        return std::string(begin, *length);
    }
} // namespace wire

/// Заголовок кадра, общий для всех сообщений.
struct MessageHeader
{
    MessageType   type{};
    std::uint32_t id{};      // сопоставляет запрос и ответ

    bool operator==(const MessageHeader&) const = default;

    std::vector<std::byte> serialize() const
    {
        std::vector<std::byte> out;
        out.reserve(headerSize);

        wire::putU8(out, version);
        wire::putU8(out, static_cast<std::uint8_t>(type));
        wire::putU32(out, id);

        return out;
    }

    static std::optional<MessageHeader> deserialize(std::span<const std::byte> in, std::size_t& pos)
    {
        const auto seenVersion = wire::getU8(in, pos);
        if (!seenVersion || *seenVersion != version)
            return std::nullopt;

        const auto seenType = wire::getU8(in, pos);
        if (!seenType)
            return std::nullopt;

        const auto seenId = wire::getU32(in, pos);
        if (!seenId)
            return std::nullopt;

        return MessageHeader{ static_cast<MessageType>(*seenType), *seenId };
    }

    static std::optional<MessageHeader> deserialize(std::span<const std::byte> in)
    {
        std::size_t pos = 0;
        return deserialize(in, pos);
    }
};

/// Вид на кадр внутри zmq-сообщения без копирования.
inline std::span<const std::byte> asBytes(const zmq::message_t& message)
{
    return { static_cast<const std::byte*>(message.data()), message.size() };
}

} // namespace protocol
