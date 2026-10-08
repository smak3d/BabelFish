#include "Server.h"

#include <charconv>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>

#include <windows.h>

namespace
{
    constexpr std::uint16_t defaultPort = 5555;

    // Порт из argv: "BabelFishServer [port]". false — не разобрано.
    bool parsePort(const char* text, std::uint16_t& port)
    {
        const std::string_view view(text);
        unsigned value = 0;

        const auto [end, error] =
            std::from_chars(view.data(), view.data() + view.size(), value);

        if (error != std::errc{} || end != view.data() + view.size())
            return false;

        if (value == 0 || value > 65535)
            return false;

        port = static_cast<std::uint16_t>(value);
        return true;
    }
}

int main(int argc, char* argv[])
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    std::uint16_t port = defaultPort;

    if (argc > 1 && !parsePort(argv[1], port))
    {
        std::cerr << "Usage: " << argv[0] << " [port]" << std::endl;
        return 1;
    }

    try
    {
        // Конструкция слушает порт сразу (IdleState::enter), run()
        // разворачивает конвейер; остановка — Ctrl+C.
        Server server("tcp://*:" + std::to_string(port));
        server.run();
    }
    catch (const std::exception& error)
    {
        // Модели (Transcriber/LocalAssistant) грузятся лениво, так что до
        // этого доходит редко; ловим на всякий случай.
        std::cerr << "Fatal error: " << error.what() << std::endl;
        return 1;
    }

    return 0;
}
