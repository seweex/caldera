#pragma once

#include <caldera/fwd.h>

#include <string>
#include <vulkan/vulkan.hpp>

namespace caldera::detail
{
    [[nodiscard]] int family_to_index(FamilyType family) noexcept;

    [[noreturn]] void fatal(const char* msg);
    [[noreturn]] void fatal(std::string const& msg);

    void assert_result(vk::Result result, const char* failedMsg);
    void assert_result(vk::Result result, std::string const& failedMsg);
}