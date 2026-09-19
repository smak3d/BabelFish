#pragma once

#include <cstdint>
#include <vector>

class Recorder
{
    private:
        std::vector<int16_t> buffer;
        bool is_recording = false;

        int sampleRate;
        int channels;

    public:
        Recorder(int sampleRate, int channels);
        ~Recorder();
        
        void startRecording();
        bool stopRecording();
        void update();

        const std::vector<int16_t>& getAudio() const;
  
};