
set(CALDERA_FRAMEWORK_CPP_FILES
    ${CMAKE_CURRENT_LIST_DIR}/src/allocator.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/commands.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/fence.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/semaphore.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/utility.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/view.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/vma_impl.cpp
)

add_library(caldera-framework STATIC ${CALDERA_FRAMEWORK_CPP_FILES})

target_include_directories(caldera-framework PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include/)

target_link_libraries(caldera-framework PUBLIC
    Vulkan::Headers
    GPUOpen::VulkanMemoryAllocator)