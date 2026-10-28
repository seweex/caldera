#pragma once

#include <expected>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include <caldera/detail/graph_allocator.hpp>
#include <caldera/fwd.hpp>
#include <caldera/structs.hpp>

namespace caldera {
/**
 * @brief Class of created render graph object
 */
class RenderGraph {
    explicit RenderGraph(FamilyNumbers const &families, detail::GraphAllocator const &allocator,
                         detail::CompiledGraph &&compiled, detail::GraphResources &&resources) noexcept;

  public:
    /**
     * @brief Compiles and allocated a render graph according to specified
     * graph intermediate representation
     *
     * @param device Vulkan device handle to allocate resources with
     * @param allocator VMA allocator handle created with specified device
     * @param families Queue family indices of the specified device
     * @param ir Description of the graph
     *
     * @return RenderGraph object if creation is successful, vk::Result code
     * of occurred error otherwise
     */
    [[nodiscard]] static std::expected<RenderGraph, vk::Result>
    create(vk::Device device, VmaAllocator allocator, FamilyNumbers const &families, GraphIR const &ir);

    ~RenderGraph() noexcept;

    RenderGraph(RenderGraph &&) noexcept;
    RenderGraph &operator=(RenderGraph &&) noexcept;

    RenderGraph(RenderGraph const &) = delete;
    RenderGraph &operator=(RenderGraph const &) = delete;

  private:
    FamilyNumbers m_families;
    detail::GraphAllocator m_allocator;

    detail::CompiledGraph m_compiled;
    detail::GraphResources m_resources;

    friend class GraphExecutor;
    friend class GraphProperties;
};
} // namespace caldera
