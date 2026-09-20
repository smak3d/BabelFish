#include "BabelFish.h"

#include <atomic>
#include <exception>
#include <iostream>
#include <thread>
#include <windows.h>

int main()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    std::cout << "STARTED\n";
    std::cout << "Press ENTER...\n";
    std::cin.get();

    std::cout << "ENTER RECEIVED\n";
    try
    {
        BabelFish app;
        

        std::cout << "BabelFish is ready.\n"
                     "Press ENTER to start recording..."
                  << std::endl;
        std::cin.get();

        app.handleEvent(Event::BUTTON_PRESSED, true);

        // Read audio on a worker thread while the main thread waits for
        // the user. State transitions occur only after this thread stops.
        std::atomic_bool recording{true};
        std::jthread recordingLoop([&app, &recording]
        {
            while (recording.load())
            {
                app.update();
            }
        });

        std::cout << "Recording... Press ENTER to stop." << std::endl;
        std::cin.get();

        recording.store(false);
        recordingLoop.join();

        State* state = app.getCurrentState();
        if (!state)
        {
            std::cerr << "Application has no active state." << std::endl;
            return 1;
        }

        // RecordingState closes the stream and launches the synchronous
        // transcription and assistant stages.
        state->handleEvent(Event::BUTTON_RELEASED);

        std::cout << "\nTranscription:\n"
                  << app.getTranscription()
                  << std::endl;

        std::cout << "\nAssistant response:\n"
                  << app.getResponse()
                  << std::endl;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Fatal error: " << error.what() << std::endl;
        return 1;
    }

    return 0;
}
