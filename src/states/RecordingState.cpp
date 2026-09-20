#include "RecordingState.h"

#include "BabelFish.h"

RecordingState::RecordingState(
    BabelFish& app,
    Recorder& recorder
)
    : app(app),
      recorder(recorder)
{
}

void RecordingState::enter()
{
    recorder.startRecording();
}

void RecordingState::update()
{
    recorder.update();
}

void RecordingState::exit()
{
}

void RecordingState::handleEvent(Event event)
{
    if (event != Event::BUTTON_RELEASED)
        return;

    bool success = recorder.stopRecording();

    if (success)
        app.setAudio(recorder.getAudio());

    app.handleEvent(
        Event::RECORDING_FINISHED,
        success
    );
}