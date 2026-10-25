include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

add_executable(caldera-tests
    ${CMAKE_CURRENT_LIST_DIR}/src/graph_allocator.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/graph_compiler.cpp
)

target_compile_definitions(caldera-render-graph PUBLIC VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1)
target_include_directories(caldera-render-graph PUBLIC ${CMAKE_HOME_DIRECTORY}/render-graph/src)

target_link_libraries(caldera-tests PRIVATE
        GTest::gtest_main
        caldera-render-graph
)

set_target_properties(caldera-tests PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF)

include(GoogleTest)
gtest_discover_tests(caldera-tests)