#include "Transcriber.h"
#include "whisper.h"
#include <stdexcept>

namespace
{
    whisper_context* context = nullptr;

    const char* modelPath = "external/models/for-tests-ggml-large.bin";
}

Transcriber::Transcriber(int sampleRate)
    : sampleRate(sampleRate)
{
    whisper_context_params contextParams =
    whisper_context_default_params();
    context = whisper_init_from_file_with_params(
        modelPath,
        contextParams
    );
        
    if (!context)
    {
        throw std::runtime_error("Failed to initialize whisper model");
    }
    
}

Transcriber::~Transcriber()
{
    if (context)
    {
        whisper_free(context);
        context = nullptr;
    }
}

std::string Transcriber::transcribe(const std::vector<int16_t>& audioData)
{

    std::vector<float> audioFloat(audioData.size());

    for (size_t i = 0; i < audioData.size(); ++i)
    {
        audioFloat[i] = static_cast<float>(audioData[i]) / 32768.0f;
    }

    //recognition parameters

    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);

    params.language = "auto";
    params.print_progress = false;
    params.print_realtime = false;
    params.print_timestamps = false;

    // Run the transcription
    int result = whisper_full(
        context, 
        params, 
        audioFloat.data(), 
        audioFloat.size()
    );

    if (result != 0)
    {
        whisper_free(context);
        throw std::runtime_error("Failed to run whisper_full");
    }

    // Get the transcription result
    std::string transcription;

    int segmentCount = whisper_full_n_segments(context);
    for(int i = 0; i < segmentCount; ++i)
    {
        transcription += whisper_full_get_segment_text(context, i);

    }

    whisper_free(context);

    return transcription;
}