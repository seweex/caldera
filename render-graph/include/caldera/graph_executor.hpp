#pragma once

#include <expected>
#include <vulkan/vulkan.hpp>
#include <caldera/structs.hpp>

namespace caldera
{
    class GraphExecutor
    {
    public:
        [[nodiscard]] std::expected<SubmittableGraph, vk::Result>
        execute(
            RenderGraph const& graph,
            ExecutionContext const& context,
            ExecutionMemory& memory);
    };
}
