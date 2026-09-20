#include "AssistantState.h"

#include "BabelFish.h"


AssistantState::AssistantState(BabelFish& app, Assistant& assistant)
    : app(app), assistant(assistant) 
{
}
void AssistantState::enter()
{
    std::string result =
        assistant.prompt(app.getTranscription());
        

    if (result.empty())
    {
         app.handleEvent(Event::ASSISTANT_FINISHED, false);
         return;
    }
    app.setResponse(result);

    app.handleEvent(Event::ASSISTANT_FINISHED, true);

}

void AssistantState::update()
{
    // Update logic for assistant state
}

void AssistantState::exit()
{
    // Cleanup logic for assistant state
}

void AssistantState::handleEvent(Event event)
{

}
