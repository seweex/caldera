
set(CALDERA_RENDER_GRAPH_CPP_FILES
    ${CMAKE_CURRENT_LIST_DIR}/src/access_resolver.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/command_buffer_distributor.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/graph_allocator.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/graph_compiler.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/graph_declarator.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/graph_executor.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/pass_declarator.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/render_graph.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/resource_map.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/resource_state_manager.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/structs.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/touch_translator.cpp
)

add_library(caldera-render-graph STATIC ${CALDERA_RENDER_GRAPH_CPP_FILES})

target_link_libraries(caldera-render-graph PUBLIC
    Vulkan::Vulkan
    GPUOpen::VulkanMemoryAllocator)

set_target_properties(caldera-render-graph PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    COMPILE_WARNING_AS_ERROR ON)

if(MSVC)
    target_compile_options(caldera-render-graph PRIVATE /W4 /permissive-)
else()
    target_compile_options(caldera-render-graph PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_include_directories(caldera-render-graph PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

set(TARGETS_EXPORT_NAME CalderaTargets)
set(CONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/caldera")

install(TARGETS caldera-render-graph
    EXPORT ${TARGETS_EXPORT_NAME}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/CalderaConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/CalderaConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/CalderaConfig.cmake"
    INSTALL_DESTINATION ${CONFIG_INSTALL_DIR}
)

install(
    EXPORT ${TARGETS_EXPORT_NAME}
    FILE CalderaTargets.cmake
    NAMESPACE caldera::
    DESTINATION ${CONFIG_INSTALL_DIR}
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/CalderaConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/CalderaConfigVersion.cmake"
    DESTINATION ${CONFIG_INSTALL_DIR}
)