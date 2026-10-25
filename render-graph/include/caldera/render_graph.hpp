#pragma once

#include <caldera/fwd.h>

#include <caldera/structs.hpp>
#include <caldera/detail/graph_allocator.hpp>

namespace caldera
{
    class RenderGraph
    {
        explicit RenderGraph(
            FamilyNumbers const& families,
            detail::GraphAllocator const& allocator,
            detail::CompiledGraph&& compiled,
            detail::GraphResources&& resources) noexcept;

    public:
        [[nodiscard]] static std::expected<RenderGraph, vk::Result>
        create(
            vk::Device device,
            VmaAllocator allocator,
            FamilyNumbers const& families,
            GraphIR const& ir);

        ~RenderGraph() noexcept;

        RenderGraph(RenderGraph&&) noexcept;
        RenderGraph& operator=(RenderGraph&&) noexcept;

        RenderGraph(RenderGraph const&) = delete;
        RenderGraph& operator=(RenderGraph const&) = delete;

    private:
        FamilyNumbers m_families;
        detail::GraphAllocator m_allocator;

        detail::CompiledGraph m_compiled;
        detail::GraphResources m_resources;

        friend class GraphExecutor;
    };
}
