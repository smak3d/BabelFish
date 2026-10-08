cmake_minimum_required(VERSION 3.20)

project(BabelFishTests)

# tests настраивается и из корневого CMakeLists.txt, и напрямую
# (cmake -S tests -B ...). В первом случае shared уже подключён корнем,
# и повторный add_subdirectory того же источника CMake запрещает.
if (NOT TARGET BabelFishShared)
    add_subdirectory(
        ${CMAKE_CURRENT_SOURCE_DIR}/../shared
        ${CMAKE_CURRENT_BINARY_DIR}/shared)
endif()

add_executable(ProtocolRoundTrip ProtocolRoundTrip.cpp)

target_link_libraries(ProtocolRoundTrip PRIVATE BabelFishShared)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ProtocolRoundTrip PRIVATE -Wall -Wextra)
endif()

# Транспорт (шаг 2) живёт в client/ и server/, но тест собирает его напрямую:
# так проверяется именно сеть, без main.cpp, Recorder, whisper и llama.
add_executable(TransportEcho
    TransportEcho.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../client/src/services/NetworkClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../server/src/services/NetworkServer.cpp
)

target_include_directories(TransportEcho PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../client/include/services
    ${CMAKE_CURRENT_SOURCE_DIR}/../server/include/services
)

target_link_libraries(TransportEcho PRIVATE BabelFishShared)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(TransportEcho PRIVATE -Wall -Wextra)
endif()

# П.3 (вход клиента + упаковка): Input -> RequestBuilder -> байты.
# Input подменяется фейком, Recorder/PortAudio и сокеты не участвуют —
# проверяется только превращение данных ввода в protocol::Request.
add_executable(InputPackaging
    InputPackaging.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../client/src/services/RequestBuilder.cpp
)

target_include_directories(InputPackaging PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../client/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../client/include/input
    ${CMAKE_CURRENT_SOURCE_DIR}/../client/include/services
)

target_link_libraries(InputPackaging PRIVATE BabelFishShared)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(InputPackaging PRIVATE -Wall -Wextra)
endif()

enable_testing()
add_test(NAME ProtocolRoundTrip COMMAND ProtocolRoundTrip)
add_test(NAME TransportEcho COMMAND TransportEcho)
add_test(NAME InputPackaging COMMAND InputPackaging)
