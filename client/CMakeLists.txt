cmake_minimum_required(VERSION 3.20)

project(BabelFishClient)

add_subdirectory(external/portaudio)

add_executable(BabelFishClient
    src/main.cpp
    src/BabelFish.cpp

    src/input/MicrophoneInput.cpp
    src/input/KeyboardInput.cpp

    src/services/Recorder.cpp
    src/services/RequestBuilder.cpp
    src/services/NetworkClient.cpp

    src/states/IdleState.cpp
    src/states/InputState.cpp
    src/states/SendingState.cpp
    src/states/RecordingState.cpp
    src/states/WaitingState.cpp
    src/states/ShowResultState.cpp
    src/states/ErrorState.cpp
)

target_include_directories(BabelFishClient PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/input
    ${CMAKE_CURRENT_SOURCE_DIR}/include/services
    ${CMAKE_CURRENT_SOURCE_DIR}/include/states
)

# BabelFishShared отдаёт include-пути shared/ и shared/protocol, заголовки
# zmq.hpp и саму libzmq — по ним и живёт NetworkClient.
target_link_libraries(BabelFishClient PRIVATE
    portaudio
    BabelFishShared
)

add_custom_command(TARGET BabelFishClient POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:portaudio>
        $<TARGET_FILE_DIR:BabelFishClient>
)