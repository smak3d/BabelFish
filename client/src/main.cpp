#include "BabelFish.h"

#include <atomic>
#include <charconv>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>

#include <windows.h>

namespace
{
    constexpr std::uint16_t defaultPort = 5555;

    // Порт из argv: "BabelFishClient [host] [port] [mic|text]". false —
    // не разобрано.
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

    // Режим ввода из argv: по умолчанию микрофон. false — не разобрано.
    bool parseMode(const char* text, InputMode& mode)
    {
        const std::string_view view(text);

        if (view == "mic" || view == "microphone")
        {
            mode = InputMode::Microphone;
            return true;
        }

        if (view == "text" || view == "keyboard")
        {
            mode = InputMode::Keyboard;
            return true;
        }

        return false;
    }

    // Ожидание команды в консоли: одна строка. false — выход (q) либо
    // поток ввода закрылся (запуск с закопченным stdin).
    bool waitForStart()
    {
        std::string line;
        if (!std::getline(std::cin, line))
            return false;

        return !(line == "q" || line == "Q");
    }
}

int main(int argc, char* argv[])
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    std::string host = "127.0.0.1";
    std::uint16_t port = defaultPort;
    InputMode mode = InputMode::Microphone;

    if (argc > 1)
        host = argv[1];

    if (argc > 2 && !parsePort(argv[2], port))
    {
        std::cerr << "Usage: " << argv[0] << " [host] [port] [mic|text]" << std::endl;
        return 1;
    }

    if (argc > 3 && !parseMode(argv[3], mode))
    {
        std::cerr << "Usage: " << argv[0] << " [host] [port] [mic|text]" << std::endl;
        return 1;
    }

    try
    {
        // Сокет ставится в конструкторе (IdleState::enter ->
        // ensureConnected): zmq делает connect() асинхронно, достижимость
        // сервера проверит первый же send()/receive().
        BabelFish app(host, port, mode);

        std::cout << "BabelFish is ready (" << host << ":" << port << ").\n";

        // Одна итерация цикла — один запрос: старт, RECORDING, затем
        // SENDING -> WAITING -> RESULT крутят сами себя, пока цикл не
        // вернётся в IdleState (после показа результата либо после
        // ErrorState, который переподключается и уходит в Idle).
        while (waitForStart())
        {
            app.handleEvent(Event::BUTTON_PRESSED, true);

            if (mode == InputMode::Microphone)
            {
                // RecordingState пишет сэмплы в update(), пока главный
                // поток ждёт стопа: Enter останавливает запись и уносит
                // цикл в SendingState (RecordingState::handleEvent).
                std::cout << "Recording... Press ENTER to stop." << std::endl;

                std::atomic_bool recording{true};
                std::jthread recordingLoop([&app, &recording]
                {
                    while (recording.load())
                    {
                        app.update();
                    }
                });

                std::string line;
                std::getline(std::cin, line);

                recording.store(false);
                recordingLoop.join();

                State* state = app.getCurrentState();
                if (state)
                    state->handleEvent(Event::BUTTON_RELEASED);
            }

            while (!app.isIdle())
            {
                app.update();
            }
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << "Fatal error: " << error.what() << std::endl;
        return 1;
    }

    return 0;
}
