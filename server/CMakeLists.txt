cmake_minimum_required(VERSION 3.20)

project(BabelFishServer)

# GPU-бэкенд моделей: включается опцией -DBABELFISH_WITH_CUDA=ON и работает
# только под MSVC — nvcc на Windows не принимает MinGW-g++ в качестве
# хост-компилятора. Поэтому у сервера появляется второе дерево сборки
# (build-cuda/), остальная часть проекта от этого не зависит.
option(BABELFISH_WITH_CUDA
       "Собирать whisper.cpp и llama.cpp с CUDA-бэкендом" OFF)

if (BABELFISH_WITH_CUDA)
    # Опция назначения и llama.cpp, и whisper.cpp (у whisper старое имя
    # WHISPER_CUDA теперь просто превращается в неё с предупреждением).
    set(GGML_CUDA ON CACHE BOOL "" FORCE)
endif()

add_subdirectory(external/llama.cpp)
add_subdirectory(external/whisper.cpp)

add_executable(BabelFishServer
    src/main.cpp
    src/Server.cpp

    src/api/ServerAPI.cpp

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
)

# BabelFishShared отдаёт include-пути shared/ и shared/protocol, заголовки
# zmq.hpp и саму libzmq — по ним и живёт NetworkServer.
target_link_libraries(BabelFishServer PRIVATE
    whisper
    llama
    BabelFishShared
)