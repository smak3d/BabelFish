cmake_minimum_required(VERSION 3.20)

project(BabelFishShared)

# ---------------------------------------------------------------------------
# ZeroMQ: libzmq (сборка из исходников) + cppzmq (только заголовок zmq.hpp)
# ---------------------------------------------------------------------------

# libzmq 4.3.5 объявляет cmake_minimum_required(VERSION 3.0.2), а CMake 4.x
# совместимость ниже 3.5 убрал. Пол версии поднимаем только внутри этого
# поддерева — как обычную переменную, она наружу не утекает.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# Опции libzmq вынуждены идти в кэш принудительно: версия политики у libzmq
# 3.5, то есть CMP0077 OLD, и option() обычную переменную проигнорирует.
# После configure эти записи убираем, чтобы не протекли в client/server.
set(BabelFishZmqCacheOptions
    BUILD_SHARED
    BUILD_STATIC
    BUILD_TESTS
    WITH_DOCS
    WITH_TLS
    ENABLE_DRAFTS)

set(BUILD_SHARED  OFF CACHE BOOL "" FORCE)   # только статика: в релизе нет libzmq.dll
set(BUILD_STATIC  ON  CACHE BOOL "" FORCE)
set(BUILD_TESTS   OFF CACHE BOOL "" FORCE)
set(WITH_DOCS     OFF CACHE BOOL "" FORCE)   # иначе лезет в pandoc
set(WITH_TLS      OFF CACHE BOOL "" FORCE)   # gnutls не нужен
set(ENABLE_DRAFTS OFF CACHE BOOL "" FORCE)

include(FetchContent)

FetchContent_Declare(libzmq
    GIT_REPOSITORY https://github.com/zeromq/libzmq.git
    GIT_TAG        v4.3.5
    GIT_SHALLOW    TRUE)

# ipc_address.hpp выбирает заголовки по `#if defined _MSC_VER`: на MSVC идёт
# <afunix.h>, на всех остальных — <sys/socket.h>. У MinGW _MSC_VER нет, а
# проверка `winsock2.h;afunix.h` у этой сборки проходит — то есть ZMQ_HAVE_IPC
# включается и сборка падает на sys/socket.h. Транспорт ipc:// нам не нужен
# (пойдём по tcp://), поэтому гасим заранее: check_include_files результат
# в кэш пишет и при уже заданной переменной проверку не запускает.
set(ZMQ_HAVE_IPC OFF CACHE INTERNAL "ipc:// на MinGW не собирается и не нужен" FORCE)

FetchContent_MakeAvailable(libzmq)

foreach(BabelFishZmqOption IN LISTS BabelFishZmqCacheOptions)
    unset(${BabelFishZmqOption} CACHE)
endforeach()
unset(BabelFishZmqCacheOptions)

# cppzmq — header-only (zmq.hpp). Его собственный CMakeLists внутри делает
# find_package(ZeroMQ) и install(EXPORT), а при FetchContent это падает, да и
# file(DOWNLOAD) с raw.githubusercontent.com здесь упирается в TLS. Поэтому
# тянем репозиторий через git, а SOURCE_SUBDIR с несуществующей папкой
# запрещает add_subdirectory — остаются только заголовки.
FetchContent_Declare(cppzmq
    GIT_REPOSITORY https://github.com/zeromq/cppzmq.git
    GIT_TAG        v4.11.0
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  cppzmq-headers-only)

FetchContent_MakeAvailable(cppzmq)

add_library(cppzmq INTERFACE)
target_include_directories(cppzmq INTERFACE ${cppzmq_SOURCE_DIR})
target_link_libraries(cppzmq INTERFACE libzmq-static)

# ---------------------------------------------------------------------------
# BabelFishShared — то, что видят client, server и tests
# ---------------------------------------------------------------------------

add_library(BabelFishShared INTERFACE)

target_include_directories(BabelFishShared INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/protocol)

target_link_libraries(BabelFishShared INTERFACE cppzmq)
target_compile_features(BabelFishShared INTERFACE cxx_std_20)
