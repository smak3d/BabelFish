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

# MSVC по умолчанию отдаёт за __cplusplus значение 199711L, пока его не
# попросишь /Zc:__cplusplus. llama.cpp по этому значению выбирает макрос
# LU8() (см. llama-chat.cpp), и без флага в C++20-режиме он берёт ветку с
# u8""-литералами — сборка падает на несовместимости char8_t/char*.
# GCC сообщает настоящий номер стандарта, ему флаг не нужен.
# Флаг применим только к C++: nvcc принимает его за входной файл и падает
# с "A single input file is required...".
if (MSVC)
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>")
endif()

add_subdirectory(shared)
add_subdirectory(client)
add_subdirectory(server)
add_subdirectory(tests)
