cmake_minimum_required(VERSION 3.20)

project(BabelFishShared)

# Header-only protocol library shared by client and server.
add_library(BabelFishShared INTERFACE)

target_include_directories(BabelFishShared INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/protocol
)

target_compile_features(BabelFishShared INTERFACE cxx_std_20)
