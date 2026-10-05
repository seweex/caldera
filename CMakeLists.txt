cmake_minimum_required(VERSION 3.21...4.3.0)

string(CONCAT CALDERA_PROJECT_DESCRIPTION
    "Caldera is an implementation of Vulkan Render Graph."
    "It removes the need to manually manage barriers and simplifies resource state control."
    "It is written on modern C++20")

project(caldera
    DESCRIPTION "${CALDERA_PROJECT_DESCRIPTION}"
    VERSION 0.0.1
    LANGUAGES CXX)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(STATUS "Enabling ASan and UBSan")

    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)

    add_link_options(-fsanitize=address,undefined)
endif()

find_program(IWYU_PATH NAMES include-what-you-use iwyu)
if(IWYU_PATH)
    set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE ${IWYU_PATH};-Xiwyu;--no_comments)
endif()

find_package(Vulkan REQUIRED)
find_package(VulkanMemoryAllocator REQUIRED)

add_subdirectory(render-graph)
add_subdirectory(examples/creating)