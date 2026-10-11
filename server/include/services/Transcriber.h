#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct whisper_context;

class Transcriber
{

    public:
        // Модель грузится в конструкторе — на месте, при старте сервера,
        // а не по первому запросу. device >= 0 — индекс GPU (0 = первая
        // карта), device < 0 — CPU. Бросает исключение, если файла модели
        // нет или бэкенд не поднялся.
        Transcriber(int sampleRate, std::string modelPath, int device);
        ~Transcriber();

        // Контекст whisper не копируется, владение — только у одного
        // объекта.
        Transcriber(const Transcriber&) = delete;
        Transcriber& operator=(const Transcriber&) = delete;

        std::string transcribe(const std::vector<int16_t>& audioData);

        const std::string& getPath() const;

    private:
        int sampleRate;
        std::string modelPath;
        whisper_context* context = nullptr;
};
