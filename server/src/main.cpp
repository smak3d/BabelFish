#include "Server.h"

#include "ModelSettings.h"

#include <llama.h>
#include <whisper.h>
#include <ggml.h>

#include <charconv>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include <windows.h>

namespace
{
    // Собственные логи llama/whisper при загрузке заливают консоль сотнями
    // строк ("loading tensor blk.N.attn_q.weight"). Это дерьмо данных, а
    // не ход работы: оставляем только предупреждения и ошибки, наши
    // строки "[server] ..." читаются и без них.
    //
    // Уровни в ggml монотонны: NONE=0, DEBUG=1, INFO=2, WARN=3, ERROR=4,
    // CONT=5 — поэтому пропускать надо level >= WARN, а не <=.
    void quietModelLogs()
    {
        llama_log_set([](enum ggml_log_level level, const char* text, void*)
        {
            if (level >= GGML_LOG_LEVEL_WARN)
                std::cerr << text;
        }, nullptr);

        whisper_log_set([](enum ggml_log_level level, const char* text, void*)
        {
            if (level >= GGML_LOG_LEVEL_WARN)
                std::cerr << text;
        }, nullptr);
    }

    // Список вычислительных устройств, доступных собранным бэкендам.
    // Индексы в этом списке и есть те N, что принимают --*-gpu.
    void printDevices()
    {
        const int count = static_cast<int>(ggml_backend_dev_count());

        if (count == 0)
        {
            std::cout << "[server] no GPU backends in this build, "
                         "models will run on CPU" << std::endl;
            return;
        }

        std::cout << "[server] compute devices:" << std::endl;

        for (int i = 0; i < count; ++i)
        {
            ggml_backend_dev_t device = ggml_backend_dev_get(i);

            std::cout << "  [" << i << "] "
                      << ggml_backend_dev_name(device) << " - "
                      << ggml_backend_dev_description(device) << std::endl;
        }
    }

    // false — устройства с таким индексом нет, и запрашивать его нельзя:
    // без этой проверки библиотеки молча уводят модель на CPU, а лог при
    // этом продолжает утверждать, что она на GPU.
    bool deviceExists(int device)
    {
        return device < 0 ||
               device < static_cast<int>(ggml_backend_dev_count());
    }

    constexpr std::uint16_t defaultPort = 5555;

    // Устройство: неотрицательное число (индекс GPU) либо "cpu".
    bool parseInt(std::string_view text, long& value)
    {
        if (text.empty())
            return false;

        const auto [end, error] =
            std::from_chars(text.data(), text.data() + text.size(), value);

        return error == std::errc{} && end == text.data() + text.size();
    }

    bool parseDevice(std::string_view text, int& device)
    {
        if (text == "cpu")
        {
            device = ModelSettings::deviceCpu;
            return true;
        }

        long value = 0;
        if (!parseInt(text, value) || value < 0)
            return false;

        device = static_cast<int>(value);
        return true;
    }

    // "gpu 0" / "cpu" — для одинаковых строк в логах.
    std::string deviceName(int device)
    {
        return device >= 0 ? "gpu " + std::to_string(device) : "cpu";
    }

    std::string usage(const char* program)
    {
        const ModelSettings defaults;

        return
            "Usage: " + std::string(program) + " [port] [options]\n"
            "\n"
            "  --transcriber-model <path>      whisper model (default: " + defaults.transcriberModel + ")\n"
            "  --transcriber-gpu <N|cpu>       device for transcription (default: " + deviceName(defaults.transcriberDevice) + ")\n"
            "  --assistant-model <path>        GGUF model (default: " + defaults.assistantModel + ")\n"
            "  --assistant-gpu <N|cpu>         device for the assistant (default: " + deviceName(defaults.assistantDevice) + ")\n"
            "  --assistant-layers <N|all>      how many layers go to VRAM, the rest\n"
            "                                  stay in RAM (default: all)\n"
            "  --help                          show this message\n";
    }

    struct ServerConfig
    {
        std::uint16_t port = defaultPort;
        ModelSettings models;
    };

    // false — аргументы некорректны, usage уже напечатан.
    bool parseArguments(int argc, char* argv[], ServerConfig& config)
    {
        for (int i = 1; i < argc; ++i)
        {
            const std::string_view argument(argv[i]);

            if (argument == "--help" || argument == "-h")
            {
                std::cout << usage(argv[0]);
                std::exit(0);
            }

            // Всё, что не опция — порт (только первый аргумент).
            if (argument.rfind("--", 0) != 0)
            {
                long value = 0;
                if (i != 1 || !parseInt(argument, value) ||
                    value == 0 || value > 65535)
                {
                    std::cerr << "Invalid port: " << argument << "\n\n"
                              << usage(argv[0]);
                    return false;
                }

                config.port = static_cast<std::uint16_t>(value);
                continue;
            }

            if (i + 1 >= argc)
            {
                std::cerr << "Missing value for " << argument << "\n\n"
                          << usage(argv[0]);
                return false;
            }

            const std::string_view value(argv[++i]);

            if (argument == "--transcriber-model")
                config.models.transcriberModel = value;
            else if (argument == "--assistant-model")
                config.models.assistantModel = value;
            else if (argument == "--transcriber-gpu")
            {
                if (!parseDevice(value, config.models.transcriberDevice))
                {
                    std::cerr << "Invalid device: " << value << "\n\n"
                              << usage(argv[0]);
                    return false;
                }
            }
            else if (argument == "--assistant-gpu")
            {
                if (!parseDevice(value, config.models.assistantDevice))
                {
                    std::cerr << "Invalid device: " << value << "\n\n"
                              << usage(argv[0]);
                    return false;
                }
            }
            else if (argument == "--assistant-layers")
            {
                if (value == "all")
                {
                    config.models.assistantLayers = -1;
                }
                else
                {
                    long layers = 0;
                    if (!parseInt(value, layers) || layers < 0)
                    {
                        std::cerr << "Invalid layer count: " << value << "\n\n"
                                  << usage(argv[0]);
                        return false;
                    }

                    config.models.assistantLayers = static_cast<int>(layers);
                }
            }
            else
            {
                std::cerr << "Unknown option: " << argument << "\n\n"
                          << usage(argv[0]);
                return false;
            }
        }

        return true;
    }
}

int main(int argc, char* argv[])
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    ServerConfig config;

    if (!parseArguments(argc, argv, config))
        return 1;

    quietModelLogs();
    printDevices();

    if (!deviceExists(config.models.transcriberDevice))
    {
        std::cerr << "--transcriber-gpu: no such device: "
                  << config.models.transcriberDevice << std::endl;
        return 1;
    }

    if (!deviceExists(config.models.assistantDevice))
    {
        std::cerr << "--assistant-gpu: no such device: "
                  << config.models.assistantDevice << std::endl;
        return 1;
    }

    try
    {
        // Модели грузятся ДО того, как сервер начнёт слушать порт: первый
        // запрос клиента уходит в обработку сразу, а проблема с моделью
        // (нет файла, не хватило VRAM) видна уже при запуске.
        Server server("tcp://*:" + std::to_string(config.port), config.models);

        server.preloadModels();

        // run() не возвращается; остановка — Ctrl+C.
        server.run();
    }
    catch (const std::exception& error)
    {
        std::cerr << "Fatal error: " << error.what() << std::endl;
        return 1;
    }

    return 0;
}
