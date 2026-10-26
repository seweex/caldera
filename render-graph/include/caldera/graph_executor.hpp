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
};
} // namespace caldera
