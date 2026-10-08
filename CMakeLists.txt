cmake_minimum_required(VERSION 3.20)

project(BabelFish)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(shared)
add_subdirectory(client)
add_subdirectory(server)
