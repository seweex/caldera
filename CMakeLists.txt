cmake_minimum_required(VERSION 3.5...4.3.0)

string(CONCAT CALDERA_PROJECT_DESCRIPTION
    "Caldera is an implementation of Vulkan Render Graph."
    "It removes the need to manually manage barriers and simplifies resource state control."
    "It is written on modern C++20")

project(caldera
    DESCRIPTION "${CALDERA_PROJECT_DESCRIPTION}"
    VERSION 0.0.1
    LANGUAGES CXX)

# if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  #  message(STATUS "Enabling ASan and UBSan")

   # add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)

    #add_link_options(-fsanitize=address,undefined)
# endif()

include(cmake/ShaderCompile.txt)

if(MSVC)
    add_compile_options(/EHs-c- /GR-)
    add_compile_definitions(_HAS_EXCEPTIONS=0)
else()
    add_compile_options(-fno-exceptions -fno-rtti)
endif()

find_package(Git REQUIRED)
find_package(glfw3 REQUIRED)
find_package(Vulkan REQUIRED)
find_package(VulkanMemoryAllocator REQUIRED)

add_subdirectory(external/vk-bootstrap)
add_subdirectory(render-graph)
target_compile_definitions(caldera-render-graph PUBLIC VULKAN_HPP_NO_EXCEPTIONS GLFW_INCLUDE_VULKAN)

add_subdirectory(framework)
add_subdirectory(examples/01_basic_graph)
add_subdirectory(examples/02_gravity_simulation)
add_subdirectory(examples/03_simulation)

enable_testing()
add_subdirectory(tests)