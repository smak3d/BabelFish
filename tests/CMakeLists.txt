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

enable_testing()
add_test(NAME ProtocolRoundTrip COMMAND ProtocolRoundTrip)
