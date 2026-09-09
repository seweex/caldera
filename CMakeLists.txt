cmake_minimum_required(VERSION 3.21...4.3.0)

string(CONCAT CALDERA_PROJECT_DESCRIPTION
    "Caldera is an implementation of Vulkan Render Graph."
    "It removes the need to manually manage barriers and simplifies resource state control."
    "It is written on modern C++20")

project(caldera
    DESCRIPTION "${CALDERA_PROJECT_DESCRIPTION}"
    VERSION 0.0.1
    LANGUAGES CXX)

add_executable(hello-world src/main.cpp)