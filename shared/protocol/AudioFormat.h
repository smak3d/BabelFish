#pragma once

#include "MessageType.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace protocol
{

/// Формат аудио, которым обмениваются две машины.
///
/// В монолите sampleRate=16000 был неявной общей константой: клиент строил
/// `Recorder recorder(16000, 1)`, сервер строил `Transcriber transcriber(16000)`
/// — и совпадение это никто не проверял. Теперь формат едет вместе с запросом,
/// и расхождение становится либо корректной ошибкой, либо невозможным.
struct AudioFormat
{
    std::uint32_t sampleRate{};
    std::uint16_t channels{};

    /// sampleRate:u32 + channels:u16
    static constexpr std::size_t wireSize = 6;

    bool operator==(const AudioFormat&) const = default;

    std::vector<std::byte> serialize() const
    {
        std::vector<std::byte> out;
        out.reserve(wireSize);

        wire::putU32(out, sampleRate);
        wire::putU16(out, channels);

        return out;
    }

    static std::optional<AudioFormat> deserialize(std::span<const std::byte> in, std::size_t& pos)
    {
        const auto seenSampleRate = wire::getU32(in, pos);
        if (!seenSampleRate)
            return std::nullopt;

        const auto seenChannels = wire::getU16(in, pos);
        if (!seenChannels)
            return std::nullopt;

        return AudioFormat{ *seenSampleRate, *seenChannels };
    }

    static std::optional<AudioFormat> deserialize(std::span<const std::byte> in)
    {
        std::size_t pos = 0;
        return deserialize(in, pos);
    }
};

} // namespace protocol
