#include <caldera/render_graph.h>
#include "detail/render_graph.h"

#include <cassert>

namespace caldera
{
    /* Allocator Attorney */

    bool RenderGraph::AllocatorAttorney::is_allocated(RenderGraph const& graph) noexcept {
        return graph.m_impl->isAllocated;
    }

    void RenderGraph::AllocatorAttorney::push_allocated(
        RenderGraph& graph, detail::AllocatedGraph&& allocated) noexcept
    {
        graph.m_impl->allocated = std::move(allocated);
        graph.m_impl->isAllocated = true;
    }

    detail::AllocatedGraph RenderGraph::AllocatorAttorney::extract_allocated(
        RenderGraph& graph) noexcept
    {
        auto& impl = *graph.m_impl;
        impl.isAllocated = false;

        return std::move(impl.allocated);
    }

    /* Compiler Attorney */

    bool RenderGraph::CompilerAttorney::is_compiled(RenderGraph const& graph) noexcept {
        return graph.m_impl->isCompiled;
    }

    void RenderGraph::CompilerAttorney::push_compiled(
        RenderGraph& graph, detail::CompiledGraph&& compiled) noexcept
    {
        assert(!graph.m_impl->isCompiled);

        graph.m_impl->isCompiled = true;
        graph.m_impl->compiled = std::move(compiled);
    }

    detail::CompiledGraph RenderGraph::CompilerAttorney::extract_compiled(
        RenderGraph& graph) noexcept
    {
        auto& impl = *graph.m_impl;
        impl.isCompiled = false;

        return std::move(impl.compiled);
    }

    /* Executor Attorney */

    detail::CompiledGraph const& RenderGraph::ExecutorAttorney::get_compiled(
        RenderGraph const &graph) noexcept
    {
        assert(graph.m_impl->isCompiled);
        return graph.m_impl->compiled;
    }

    detail::AllocatedGraph const& RenderGraph::ExecutorAttorney::get_allocated(
        RenderGraph const &graph) noexcept
    {
        assert(graph.m_impl->isAllocated);
        return graph.m_impl->allocated;
    }

    /* Render Graph */

    void RenderGraph::Deleter::operator()(Impl* const impl) const noexcept
    {
        if (impl) {
            // assert(!impl->isAllocated);
            std::default_delete<Impl>{}(impl);
        }
    }

    RenderGraph::RenderGraph() :
        m_impl(new Impl(), Deleter{})
    {}

    RenderGraph::~RenderGraph() noexcept = default;

    RenderGraph::RenderGraph(RenderGraph &&) noexcept = default;
    RenderGraph & RenderGraph::operator=(RenderGraph &&) noexcept = default;
}
