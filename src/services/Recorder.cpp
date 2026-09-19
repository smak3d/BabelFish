#include "Recorder.h"

#include <portaudio.h>
#include <iostream>
#include <vector>

namespace
{
    PaStream* stream = nullptr;

    constexpr unsigned long framesPerBuffer = 256;
}


Recorder::Recorder(int sampleRate, int channels)
    : sampleRate(sampleRate), channels(channels)
{
}

Recorder::~Recorder()
{
    stopRecording();
    Pa_Terminate();
}

void Recorder::startRecording()
{
    
    buffer.clear();

    PaError error = Pa_Initialize();
    if (error!= paNoError)
    {
        std::cerr << "PortAudio init error: " << Pa_GetErrorText(error) << std::endl;
        return;
    }

    error = Pa_OpenDefaultStream(
        &stream,
        channels, // Input channels
        0,        // No output channels
        paInt16,  // Sample format
        sampleRate,
        framesPerBuffer,
        nullptr,  // No callback
        nullptr); // No user data

    if (error != paNoError)
    {
        std::cerr << "PortAudio open error: " << Pa_GetErrorText(error) << std::endl;
        stream = nullptr;
        Pa_Terminate();
        return;
    }
    error = Pa_StartStream(stream);

    if (error != paNoError)
    {
        std::cerr << "PortAudio start error: " << Pa_GetErrorText(error) << std::endl;
        Pa_CloseStream(stream);
        stream = nullptr;
        Pa_Terminate();
        return;
    }

    is_recording = true;
}

bool Recorder::stopRecording()
{
    if (!stream)
    {
        return false;
    }

    PaError result = Pa_StopStream(stream);

    if (result != paNoError)
    {
        std::cerr << "PortAudio stop error: "
                  << Pa_GetErrorText(result) << std::endl;

        Pa_CloseStream(stream);
        stream = nullptr;
        is_recording = false;

        return false;
    }

    result = Pa_CloseStream(stream);

    if (result != paNoError)
    {
        std::cerr << "PortAudio close error: "
                  << Pa_GetErrorText(result) << std::endl;

        stream = nullptr;
        is_recording = false;

        return false;
    }

    stream = nullptr;
    is_recording = false;

    return !buffer.empty();
}

void Recorder::update()
{
    if (!is_recording || !stream)
    {
        return;
    }

    std::vector<int16_t> tempBuffer(framesPerBuffer * channels);
    PaError result = Pa_ReadStream(stream, tempBuffer.data(), framesPerBuffer);

    if(result == paNoError || result  == paInputOverflowed)
    {
        buffer.insert(buffer.end(), tempBuffer.begin(), tempBuffer.end());
    }
    else
    {
        std::cerr << "PortAudio read error: " << Pa_GetErrorText(result) << std::endl;
    }
}

const std::vector<int16_t>& Recorder::getAudio() const
{
    return buffer;
}