
set(CALDERA_RENDER_GRAPH_CPP_FILES
    ${CMAKE_CURRENT_LIST_DIR}/src/access_resolver.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/command_buffer_distributor.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/graph_allocator.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/graph_compiler.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/graph_declarator.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/graph_execution_ir.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/graph_executor.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/graph_ir.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/graph_resources_ir.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/pass_declarator.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/render_graph.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/resource_map.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/resource_state_manager.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/semaphore_map.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/structs.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/usage_translator.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/utility.cpp
)

add_library(caldera-render-graph STATIC ${CALDERA_RENDER_GRAPH_CPP_FILES})

target_include_directories(caldera-render-graph PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include/)
target_include_directories(caldera-render-graph PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/)

target_link_libraries(caldera-render-graph PUBLIC
    Vulkan::Vulkan
    GPUOpen::VulkanMemoryAllocator)

set_target_properties(caldera-render-graph PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)