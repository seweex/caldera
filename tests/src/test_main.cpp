#if !VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
#error "Dynamic dispatching is required"
#endif

#include <gtest/gtest.h>
#include <vulkan/vulkan.hpp>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}