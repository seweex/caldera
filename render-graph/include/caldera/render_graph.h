#pragma once

#include <memory>
#include "fwd.h"

namespace caldera
{
    class RenderGraph
    {
    public:
        RenderGraph();
        ~RenderGraph() noexcept;

        RenderGraph(RenderGraph &&) noexcept;
        RenderGraph& operator=(RenderGraph &&) noexcept;

        RenderGraph(RenderGraph const&) = delete;
        RenderGraph& operator=(RenderGraph const&) = delete;

        struct AllocatorAttorney;
        struct CompilerAttorney;
        struct ExecutorAttorney;

    private:
        struct Impl;
        struct Deleter { void operator()(Impl*) const noexcept; };

        std::unique_ptr<Impl, Deleter> m_impl;
    };
}