
function(declare_shader_compilation SOURCE_PATH EXPORT_PATH TARGET_NAME)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    execute_process(
        COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive external/shader-link
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMAND_ERROR_IS_FATAL ANY)

    set(COMPILATION_TARGET_NAME ${TARGET_NAME}-shader-compilation)
    set(SHADER_LINK_ROOT "${CMAKE_SOURCE_DIR}/external/shader-link")

    set(COMPILATION_OUTPUT_PATH "${CMAKE_BINARY_DIR}/compiled-shaders/${TARGET_NAME}")

    file(MAKE_DIRECTORY "${COMPILATION_OUTPUT_PATH}")
    file(MAKE_DIRECTORY "${EXPORT_PATH}")

    add_custom_target(${COMPILATION_TARGET_NAME}
        COMMAND Python3::Interpreter -m shader_link
            --input  "${SOURCE_PATH}"
            --output "${CMAKE_BINARY_DIR}/compiled-shaders/${TARGET_NAME}"
            --export "${EXPORT_PATH}"
            --args   "-O --target-env=vulkan1.4"
        WORKING_DIRECTORY "${SHADER_LINK_ROOT}"
        VERBATIM)

    add_dependencies(${TARGET_NAME} ${COMPILATION_TARGET_NAME})
endfunction()