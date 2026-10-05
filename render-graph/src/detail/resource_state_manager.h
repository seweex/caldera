#pragma once

#include <vector>
#include <vulkan/vulkan.hpp>

#include <caldera/resource_state_manager.h>
#include "detail/structs.h"

namespace caldera
{
    struct ResourceStateManager::Impl
    {
        void cleanup_transient() noexcept;

        std::vector<detail::ResourceTouch> externImageTouches;
        std::vector<detail::ResourceTouch> externBufferTouches;

        std::vector<detail::ResourceTouch> transientImageTouches;
        std::vector<detail::ResourceTouch> transientBufferTouches;
    };

    struct ResourceStateManager::ExecutorAttorney
    {
        static void cleanup_transient(ResourceStateManager& manager) noexcept;

        [[nodiscard]] static detail::ResourceTouch&
        get_state(ResourceStateManager& manager, ImageID id) noexcept;

        [[nodiscard]] static detail::ResourceTouch&
        get_state(ResourceStateManager& manager, BufferID id) noexcept;

        [[nodiscard]] static detail::ResourceTouch const&
        get_state(ResourceStateManager const& manager, ImageID id) noexcept;

        [[nodiscard]] static detail::ResourceTouch const&
        get_state(ResourceStateManager const& manager, BufferID id) noexcept;
    };
}