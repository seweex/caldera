#pragma once

#include <cstdint>
#include <span>
#include <vector>
#include <vulkan/vulkan.hpp>

#include <caldera/fwd.hpp>

namespace caldera
{
    /**
     * @brief Class for distributing command buffers to the
     * graph during an execution
     */
    class CommandBufferDistributor
    {
    public:
        /**
         * @brief Pushes command buffers to the family pool
         * @param family Queue family index of the pool provided command buffer
         * should be pushed to
         * @param buffers List of command buffers
         */
        void feed(uint32_t family, std::span<vk::CommandBuffer const> buffers);

        /**
         * @brief Marks all command buffers of a family pool reset
         * @param family Queue family index of the pool
         *
         * @note This method doesn't call the vkResetCommandBuffer and vkResetCommandPool funcs
         */
        void reset(uint32_t family) noexcept;


    private:
        [[nodiscard]] vk::CommandBuffer take_buffer(uint32_t family);

        friend class GraphExecutor;

        std::vector<detail::CommandFamily> m_commandFamilies;
    };
}
