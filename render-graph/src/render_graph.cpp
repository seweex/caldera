#include <expected>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include <caldera/detail/graph_allocator.hpp>
#include <caldera/detail/graph_compiler.hpp>

#include <caldera/render_graph.hpp>

namespace caldera {
RenderGraph::RenderGraph(FamilyNumbers const &families, detail::GraphAllocator const &allocator,
                         detail::CompiledGraph &&compiled, detail::GraphResources &&resources) noexcept
    : m_families(families), m_allocator(allocator), m_compiled(std::move(compiled)), m_resources(std::move(resources)) {
}

std::expected<RenderGraph, vk::Result> RenderGraph::create(vk::Device const device, VmaAllocator const allocator,
                                                           FamilyNumbers const &families, GraphIR const &ir) {
    detail::GraphCompiler graphCompiler{};
    detail::GraphAllocator graphAllocator{device, allocator, families};

    auto compiled = graphCompiler.compile(families, ir);
    auto allocated = graphAllocator.allocate(ir, compiled.trackedImages, compiled.trackedBuffers);

    if (!allocated)
        return std::unexpected{allocated.error()};

    return RenderGraph{families, graphAllocator, std::move(compiled), std::move(*allocated)};
}

RenderGraph::~RenderGraph() noexcept { m_allocator.deallocate(m_resources); }

RenderGraph::RenderGraph(RenderGraph &&) noexcept = default;
RenderGraph &RenderGraph::operator=(RenderGraph &&) noexcept = default;
} // namespace caldera
