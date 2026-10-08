#include "IdleState.h"

#include "BabelFish.h"

#include <iostream>

IdleState::IdleState(BabelFish& app)
    : app(app)
{
}

void IdleState::enter()
{
    // Соединение ставится здесь — на старте приложения и после каждого
    // возврата в Idle. Обычно сокет уже жив, и ensureConnected() —
    // дешёвая проверка isConnected(); настоящий connect() случается на
    // старте и после сетевой ошибки (там же его делает ErrorState —
    // двойная страховка не мешает, повторный вызов сокет не пересоздаёт).
    app.ensureConnected();

    std::cout << "[client] ready\n";
}

void IdleState::update()
{
    // Ждать здесь нечего: старт цикла приходит из main.cpp событием
    // BUTTON_PRESSED.
}

void IdleState::exit()
{
}

void IdleState::handleEvent(Event)
{
    // События конвейера раздаёт BabelFish::handleEvent — сюда их никто не шлёт.
}
