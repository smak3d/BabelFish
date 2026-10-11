#pragma once

#include <string>

// Где лежат модели и на каких устройствах они живут. Значения по умолчанию —
// раскладка для двухкартной машины: распознавание речи на слабой карте,
// языковая модель — на основной. Приложение запускается без аргументов, так
// что это не просто заглушки, а рабочие дефолты.
//
// Устройство: >= 0 — индекс GPU (0 = первая карта), deviceCpu — целиком на
// процессоре.
struct ModelSettings
{
    static constexpr int deviceCpu = -1;

    std::string transcriberModel = "H:/models/ggml-large-v3-turbo.bin";
    int transcriberDevice        = 1; // RTX 2070, 8 ГБ

    std::string assistantModel = "H:/models/Qwen3-4B-Q6_K.gguf";
    int assistantDevice        = 0; // RTX 5070 Ti, 16 ГБ

    // Сколько слоёв ассистента уезжает в VRAM: -1 = все, 0 = ни одного
    // (чистый CPU), N = первые N слоёв, остальное считается в оперативной
    // памяти. Учитывается только при assistantDevice >= 0.
    int assistantLayers = -1;
};
