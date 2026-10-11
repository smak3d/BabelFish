#pragma once

#include <string>
#include <vector>

struct llama_model;
struct llama_context;

class LocalAssistant
{

    public:
        // Модель грузится в конструкторе — при старте сервера, а не по
        // первому запросу. device >= 0 — индекс GPU, < 0 — CPU.
        // gpuLayers — сколько слоёв уезжает в VRAM: -1 = все, 0 = ни
        // одного (чистый CPU), N = первые N слоёв, остальное остаётся в
        // оперативной памяти. gpuLayers учитывается только при device >= 0.
        // Бросает исключение, если файла нет или памяти не хватило.
        LocalAssistant(std::string modelPath, int device, int gpuLayers = -1);
        ~LocalAssistant();

        // Контексты llama не копируются, владение — только у одного объекта.
        LocalAssistant(const LocalAssistant&) = delete;
        LocalAssistant& operator=(const LocalAssistant&) = delete;

        std::string prompt(const std::string& text);

        const std::string& getPath() const;

    private:
        std::string modelPath;
        int device;
        int gpuLayers;

        llama_model* model = nullptr;
        llama_context* context = nullptr;
};
