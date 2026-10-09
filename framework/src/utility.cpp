#include <vulkan/vulkan.hpp>
#include "detail/utility.h"

#include <iostream>
#include <ostream>

namespace caldera::framework
{
    void validate_result(VkResult result, const char* message)
    {
        if (result < VK_SUCCESS) {
            std::cerr << message << std::endl;
            std::abort();
        }
    }

    void validate_result(vk::Result result, const char* message) {
        validate_result(static_cast<VkResult>(result), message);
    }
}
