
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)

add_executable(caldera-example-creating
        ${CMAKE_CURRENT_LIST_DIR}/src/main.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/vk_context.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/vma_allocator.cpp)

target_compile_definitions(caldera-example-creating PRIVATE VULKAN_HPP_NO_EXCEPTIONS)

target_link_libraries(caldera-example-creating PRIVATE
        Vulkan::Vulkan
        caldera-render-graph)
