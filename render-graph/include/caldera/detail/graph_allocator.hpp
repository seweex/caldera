#pragma once

#include <expected>

#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>

#include <caldera/structs.hpp>

namespace caldera::detail
{
    class GraphAllocator
    {
    public:
        GraphAllocator(
            vk::Device device,
            VmaAllocator allocator,
            FamilyNumbers const& families) noexcept;

        [[nodiscard]] std::expected<GraphResources, vk::Result>
        allocate(
            GraphIR const& ir,
            ImageTrackingTable const& imageTracking,
            BufferTrackingTable const& bufferTracking);

        void deallocate(GraphResources& resources);

    private:
        vk::Device m_device;
        VmaAllocator m_allocator;
        FamilyNumbers m_families;
    };
}