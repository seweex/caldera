#pragma once

#include <caldera/render_graph.h>
#include "structs.h"

namespace caldera
{
    struct RenderGraph::Impl
    {
        bool isCompiled;
        bool isAllocated;

        detail::CompiledGraph compiled;
        detail::AllocatedGraph allocated;
    };

    struct RenderGraph::AllocatorAttorney
    {
        [[nodiscard]] static bool
        is_allocated(RenderGraph const& graph) noexcept;

        static void
        push_allocated(RenderGraph& graph, detail::AllocatedGraph&& allocated) noexcept;

        static detail::AllocatedGraph
        extract_allocated(RenderGraph& graph) noexcept;
    };

    struct RenderGraph::CompilerAttorney
    {
        [[nodiscard]] static bool
        is_compiled(RenderGraph const& graph) noexcept;

        static void
        push_compiled(RenderGraph& graph, detail::CompiledGraph&& compiled) noexcept;

        static detail::CompiledGraph
        extract_compiled(RenderGraph& graph) noexcept;
    };

    struct RenderGraph::ExecutorAttorney
    {
        [[nodiscard]] static detail::CompiledGraph const& get_compiled(RenderGraph const& graph) noexcept;
        [[nodiscard]] static detail::AllocatedGraph const& get_allocated(RenderGraph const& graph) noexcept;
    };
}