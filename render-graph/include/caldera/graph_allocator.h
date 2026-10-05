#pragma once

#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>

#include "fwd.h"
#include "structs.h"

namespace caldera
{
    class GraphAllocator
    {
    public:
        GraphAllocator(VmaAllocator allocator, FamilyNumbers families) noexcept;

        void allocate(GraphIR const& ir, RenderGraph& graph);
        void deallocate(RenderGraph& graph);

    private:
        VmaAllocator m_allocator;
        FamilyNumbers m_families;
        vk::Device m_device;
    };
}