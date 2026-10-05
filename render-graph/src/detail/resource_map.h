#pragma once

#include <vulkan/vulkan.hpp>
#include <caldera/resource_map.h>

#include "detail/structs.h"

namespace caldera
{
    struct ResourceMap::TransientAttorney
    {
        static void import_graph(
            ResourceMap& map,
            detail::AllocatedGraph const& graph);

        static void reset_transient(ResourceMap& map) noexcept;
    };

    struct ResourceMap::Impl
    {
        std::vector<vk::Image> transientImages;
        std::vector<vk::Buffer> transientBuffers;

        std::vector<vk::Image> externImages;
        std::vector<vk::Buffer> externBuffers;
    };
}
