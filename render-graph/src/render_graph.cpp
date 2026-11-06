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

RenderGraph::RenderGraph() noexcept : m_allocator(VK_NULL_HANDLE, VK_NULL_HANDLE, m_families) {}

void RenderGraph::reset() noexcept {
    if (m_allocator.is_valid()) {
        m_allocator.deallocate(m_resources);
        m_allocator.reset();
    }
}

RenderGraph::operator bool() const noexcept { return m_allocator.is_valid(); }
bool RenderGraph::operator!() const noexcept { return !this->operator bool(); }
bool RenderGraph::operator==(std::nullptr_t) const noexcept { return this->operator bool(); }
bool RenderGraph::operator!=(std::nullptr_t) const noexcept { return !this->operator bool(); }

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

RenderGraph::~RenderGraph() noexcept { reset(); }

RenderGraph::RenderGraph(RenderGraph &&other) noexcept
    : m_families(std::move(other.m_families)), m_allocator(std::move(other.m_allocator)),
      m_compiled(std::move(other.m_compiled)), m_resources(std::move(other.m_resources)) {
    other.reset();
}

RenderGraph &RenderGraph::operator=(RenderGraph &&other) noexcept {
    if (&other == this)
        return *this;

    reset();

    m_families = std::move(other.m_families);
    m_allocator = std::move(other.m_allocator);
    m_compiled = std::move(other.m_compiled);
    m_resources = std::move(other.m_resources);

    other.reset();

    return *this;
}
} // namespace caldera
