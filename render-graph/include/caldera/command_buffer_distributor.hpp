#pragma once

#include <span>
#include <vector>
#include <vulkan/vulkan.hpp>

#include <caldera/structs.hpp>

namespace caldera
{
    class CommandBufferDistributor
    {
    public:
        void feed(uint32_t family, std::span<vk::CommandBuffer const> buffers);
        void reset(uint32_t family) noexcept;

    private:
        [[nodiscard]] vk::CommandBuffer take_buffer(uint32_t family);

        friend class GraphExecutor;

        std::vector<detail::CommandFamily> m_commandFamilies;
    };
}
