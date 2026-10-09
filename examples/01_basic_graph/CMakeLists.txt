add_executable(caldera-example-basic-graph
        ${CMAKE_CURRENT_LIST_DIR}/src/main.cpp)

target_link_libraries(caldera-example-basic-graph PRIVATE
        caldera-render-graph
        caldera-framework
        Vulkan::Vulkan
        vk-bootstrap::vk-bootstrap)