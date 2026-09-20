#include "TranscribingState.h"
#include "Transcriber.h"
#include "BabelFish.h"

#include <string>



TranscribingState::TranscribingState(
    BabelFish& app, 
    Transcriber& transcriber
)
    : app(app),
      transcriber(transcriber)
{
}

void TranscribingState::enter()
{
    std::string result =
        transcriber.transcribe(app.getAudio());
    
    if(result.empty())
    {
        app.handleEvent(
            Event::TRANSCRIPTION_FINISHED,
            false
        );
        return;
    }

    app.setTranscription(result);

    app.handleEvent(
        Event::TRANSCRIPTION_FINISHED,
        true
    );

}

void TranscribingState::update()
{
    // Update logic for transcribing state
}

void TranscribingState::exit()
{
    // Cleanup logic for transcribing state
}
void TranscribingState::handleEvent(Event event)
{
    
}
