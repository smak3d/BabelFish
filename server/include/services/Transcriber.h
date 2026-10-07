#pragma once 

#include <cstdint>
#include <string>   
#include <vector>

class Transcriber 
{

    public:
        Transcriber(int sampleRate);
        ~Transcriber();

        std::string transcribe(const std::vector<int16_t>& audioData);

    private:
        int sampleRate;
};