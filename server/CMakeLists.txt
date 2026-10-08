cmake_minimum_required(VERSION 3.20)

project(BabelFishServer)

add_subdirectory(external/llama.cpp)
add_subdirectory(external/whisper.cpp)

add_executable(BabelFishServer
    src/main.cpp
    src/Server.cpp

    src/api/ServerApi.cpp

    src/services/NetworkServer.cpp
    src/services/Router.cpp
    src/services/Transcriber.cpp
    src/services/LocalAssistant.cpp
    src/services/NetAssistant.cpp

    src/states/IdleState.cpp
    src/states/ReceivingState.cpp
    src/states/ProcessingState.cpp
    src/states/SendingState.cpp
    src/states/ErrorState.cpp
)

target_include_directories(BabelFishServer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/api
    ${CMAKE_CURRENT_SOURCE_DIR}/include/services
    ${CMAKE_CURRENT_SOURCE_DIR}/include/states

    ${CMAKE_CURRENT_SOURCE_DIR}/../shared
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/protocol
)

target_link_libraries(BabelFishServer PRIVATE
    whisper
    llama
)