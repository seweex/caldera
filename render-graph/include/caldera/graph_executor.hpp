#pragma once

#include <expected>
#include <vulkan/vulkan.hpp>

#include <caldera/fwd.hpp>

namespace caldera {
/**
 * @brief Executor object for render graphs
 */
class GraphExecutor {
  public:
    /**
     * @brief Enables debug labels in command buffers
     *
     * @tparam DispatcherTy Type of vulkan dispatcher
     * @param d Vulkan dispatcher with the vkCmdBeginDebugUtilsLabelEXT
     * and vkCmdEndDebugUtilsLabelEXT functions
     */
    template <class DispatcherTy = VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>
    void enable_debug_labels(DispatcherTy const &d = VULKAN_HPP_DEFAULT_DISPATCHER) {
        m_vkCmdBeginDebugUtilsLabelEXT = d.vkCmdBeginDebugUtilsLabelEXT;
        m_vkCmdEndDebugUtilsLabelEXT = d.vkCmdEndDebugUtilsLabelEXT;

        assert((m_vkCmdBeginDebugUtilsLabelEXT == nullptr) == (nullptr == m_vkCmdEndDebugUtilsLabelEXT) &&
               "failed to get proc addresses: specified dispatcher doesn't provide required functions");
    }

    /// @brief Disables debug labels in command buffers
    void disable_debug_labels() noexcept;

    /**
     * @brief Executes a render graph object in a user-specified environment
     *
     * @param graph Render graph to execute
     * @param context Context of the render graph execution with ResourceMap,
     * CommandBufferDistributor, and ResourceStateManager classes
     * @param memory Memory of the render graph execution that will be reused
     * to minimize dynamic allocations during the game loop
     *
     * @return SubmittableGraph object if execution is successful, otherwise
     * vk::Result code of occurred error
     *
     * @warning Returned SubmittableGraph is valid until the next execution of
     * any render graph with the specified ExecutionMemory object. If you
     * want to save the SubmittableGraph, don't reuse the ExecutionMemory anymore
     * while you still need the result of this method anymore, or copy the
     * result first
     *
     * @note This method doesn't submit any vk::CommandBuffer object. It only
     * records them
     */
    [[nodiscard]] std::expected<SubmittableGraph, vk::Result>
    execute(RenderGraph const &graph, ExecutionContext const &context, ExecutionMemory &memory);

  private:
    PFN_vkCmdBeginDebugUtilsLabelEXT m_vkCmdBeginDebugUtilsLabelEXT = nullptr;
    PFN_vkCmdEndDebugUtilsLabelEXT m_vkCmdEndDebugUtilsLabelEXT = nullptr;
};
} // namespace caldera
