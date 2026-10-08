#include "InputState.h"

#include "BabelFish.h"
#include "Input.h"
#include "RequestBuilder.h"

#include <iostream>

InputState::InputState(
    BabelFish& app,
    Input& input,
    RequestBuilder& builder
)
    : app(app),
      input(input),
      builder(builder)
{
}

void InputState::enter()
{
    // Каждый цикл — с чистого листа: прошлый текст/запись и флаг
    // isFinished() не должны пережить переход в состояние.
    input.reset();
    request.reset();

    std::cout << "Entering Input State\n";
}

void InputState::update()
{
    // Опрос ввода — некий блокирующий cin.get() из main.cpp: один кадр
    // = один шаг, состояние остаётся текущим, пока Enter не завершит
    // ввод.
    const auto event = input.update();
    if (!event)
        return;

    if (*event == Event::BUTTON_PRESSED)
        return; // ввод начался, копим данные

    // BUTTON_RELEASED: данных больше не будет — собираем запрос.
    request = builder.build(input);

    if (!request)
    {
        // Пустой ввод (Enter до печати, сбой микрофона): отправлять
        // нечего, это ошибка ввода, а не сети.
        app.handleEvent(Event::BUTTON_RELEASED, false);
        return;
    }

    // Данные превратились в готовый байтовый запрос — дальше цепочка
    // решает, когда его слать (п.5).
    app.handleEvent(Event::BUTTON_RELEASED, true);
}

void InputState::exit()
{
}

void InputState::handleEvent(Event)
{
    // Вводом рулит сам Input через update(); события сюда не приходят —
    // состояние не переходит в другие по ним. Параметр без имени:
    // подавить -Wunused-parameter, обрабатывать здесь нечего.
}

const std::optional<protocol::Request>& InputState::getRequest() const
{
    return request;
}
