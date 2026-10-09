#pragma once

#include <vulkan/vulkan.h>

namespace caldera::framework
{
    void validate_result(VkResult result, const char* message);
    void validate_result(vk::Result result, const char* message);
}