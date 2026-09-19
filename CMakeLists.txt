cmake_minimum_required(VERSION 3.20)

project(BabelFish)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(external/portaudio)
add_subdirectory(external/whisper.cpp)
add_subdirectory(external/llama.cpp)

add_executable(BabelFish
    src/main.cpp     
    src/BabelFish.cpp

    src/services/Recorder.cpp
    src/services/Transcriber.cpp
    src/services/Assistant.cpp

    src/states/ErrorState.cpp
    src/states/IdleState.cpp
    src/states/RecordingState.cpp
    src/states/TranscribingState.cpp
    src/states/AssistantState.cpp
)

target_link_libraries(BabelFish PRIVATE 
    portaudio
    whisper
    llama
)

