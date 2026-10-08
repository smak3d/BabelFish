#include "ShowResultState.h"

#include "BabelFish.h"

#include <iostream>

ShowResultState::ShowResultState(
    BabelFish& app
)
    : app(app)
{
}

void ShowResultState::enter()
{
    std::cout << "[client] transcription:\n"
              << app.getTranscription()
              << "\n[client] response:\n"
              << app.getResponse()
              << std::endl;
}

void ShowResultState::update()
{
    // Показ мгновенный: результат напечатан в enter(), цикл событий
    // закрывается первым же кадром.
    app.handleEvent(Event::RESULT_SHOWN, true);
}

void ShowResultState::exit()
{
}

void ShowResultState::handleEvent(Event)
{
    // События конвейера раздаёт BabelFish::handleEvent — сюда их никто не шлёт.
}
