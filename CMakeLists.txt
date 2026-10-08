cmake_minimum_required(VERSION 3.20)

project(BabelFish)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Без этого в build/ не появляется compile_commands.json, а на него завязан
# IntelliSense в VS Code: без него редактор не знает include-пути заголовков,
# которые лежат в build/_deps (zmq.hpp и zmq.h).
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()

add_subdirectory(shared)
add_subdirectory(client)
add_subdirectory(server)
add_subdirectory(tests)
