#include "RequestBuilder.h"

namespace
{
    std::vector<std::byte> toBytes(const std::string& value)
    {
        std::vector<std::byte> bytes(value.size());
        if (!value.empty())
            std::memcpy(bytes.data(), value.data(), value.size());
        return bytes;
    }
}

RequestBuilder::RequestBuilder(std::uint32_t sampleRate, std::uint16_t channels)
    : format{ sampleRate, channels }
{
}

std::optional<protocol::Request> RequestBuilder::build(Input& input)
{
    if (!input.isFinished())
        return std::nullopt;

    const auto& data = input.data();

    switch (input.mode())
    {
        case InputMode::Microphone:
        {

            if (data.audio.empty())
                return std::nullopt;

            return buildAudio(data.audio);
        }

        case InputMode::Keyboard:
        {
            if (data.text.empty())
                return std::nullopt;

            return buildText(data.text);
        }
    }


    return std::nullopt;
}

protocol::Request RequestBuilder::buildAudio(std::span<const std::int16_t> samples)
{
    protocol::Request request;
    request.id = ++lastId;
    request.setAudio(format, samples);
    return request;
}

protocol::Request RequestBuilder::buildText(const std::string& text)
{
    protocol::Request request;
    request.id      = ++lastId;
    request.type    = protocol::MessageType::TextRequest;
    request.payload = toBytes(text);
    return request;
}

std::uint32_t RequestBuilder::nextId() const
{
    return lastId + 1;
}

const protocol::AudioFormat& RequestBuilder::getFormat() const
{
    return format;
}
