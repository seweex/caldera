#include <caldera/graph_allocator.h>
#include <caldera/render_graph.h>
#include "detail/render_graph.h"

#include <vulkan/vulkan.hpp>
#include <ranges>

#include "caldera/graph_ir.h"
#include "detail/graph_ir.h"
#include "detail/utility.h"

namespace
{
    [[nodiscard]] vk::Device retrieve_device(VmaAllocator const allocator)
    {
        VmaAllocatorInfo info;
        vmaGetAllocatorInfo(allocator, &info);

        return vk::Device{ info.device };
    }

    [[nodiscard]] std::vector<vk::Image> create_images(
        vk::Device const device,
        caldera::FamilyNumbers const& families,
        std::vector<caldera::detail::ImageCreateInfo> const& infos)
    {
        std::vector<vk::Image> result;
        result.reserve(infos.size());

        for (auto const& info : infos)
        {
            auto const family = families.get(info.family);

            vk::ImageCreateInfo vkInfo = {};
            vkInfo.flags = info.flags;
            vkInfo.imageType = info.type;
            vkInfo.format = info.format;
            vkInfo.extent = info.extent;
            vkInfo.mipLevels = info.mipLevels;
            vkInfo.arrayLayers = info.arrayLayers;
            vkInfo.samples = info.samples;
            vkInfo.tiling = vk::ImageTiling::eOptimal;
            vkInfo.usage = info.usage;
            vkInfo.sharingMode = vk::SharingMode::eExclusive;
            vkInfo.queueFamilyIndexCount = 1;
            vkInfo.pQueueFamilyIndices = &family;

            vk::Image image;
            auto const code = device.createImage(&vkInfo, nullptr, &image);

            caldera::detail::assert_result(code, "failed to create image");

            result.emplace_back(image);
        }

        return result;
    }

    [[nodiscard]] std::vector<vk::Buffer> create_buffers(
        vk::Device const device,
        caldera::FamilyNumbers const& families,
        std::vector<caldera::detail::BufferCreateInfo> const& infos)
    {
        std::vector<vk::Buffer> result;
        result.reserve(infos.size());

        for (auto const& info : infos)
        {
            auto const family = families.get(info.family);

            vk::BufferCreateInfo vkInfo = {};
            vkInfo.flags = info.flags;
            vkInfo.size = info.size;
            vkInfo.usage = info.usage;
            vkInfo.sharingMode = vk::SharingMode::eExclusive;
            vkInfo.queueFamilyIndexCount = 1;
            vkInfo.pQueueFamilyIndices = &family;

            vk::Buffer buffer;
            auto const code = device.createBuffer(&vkInfo, nullptr, &buffer);

            caldera::detail::assert_result(code, "failed to create buffer");

            result.emplace_back(buffer);
        }

        return result;
    }

    [[nodiscard]] std::vector<caldera::detail::MemoryHeap> allocate_heaps(
        VmaAllocator const allocator,
        std::vector<caldera::detail::MemoryHeapInfo> const& infos)
    {
        std::vector<caldera::detail::MemoryHeap> result;
        result.reserve(infos.size());

        for (auto const& info : infos)
        {
            VkMemoryRequirements requirements {};
            requirements.size = info.size;
            requirements.alignment = info.alignment;
            requirements.memoryTypeBits = info.memoryTypeBit;

            VmaAllocationCreateInfo constexpr createInfo {};

            VmaAllocationInfo allocationInfo {};
            VmaAllocation allocation {};

            vk::Result const code { vmaAllocateMemory(
                allocator, &requirements, &createInfo, &allocation, &allocationInfo)
            };

            caldera::detail::assert_result(code, "failed to allocate memory");

            result.emplace_back(allocation, allocationInfo.deviceMemory);
        }

        return result;
    }

    template <class TagTy>
    void bind_memory(
        vk::Device const device,
        std::vector<caldera::detail::MemoryHeap> const& heaps,
        auto const& resources,
        auto const& resourceInfos)
    {
        for (uint32_t resourceIdx = 0; resourceIdx < resources.size(); ++resourceIdx)
        {
            auto const& resource = resources[resourceIdx];
            auto const& binding = resourceInfos[resourceIdx].binding;

            auto const memory = heaps[binding.heapIdx].memory;

            if constexpr (std::same_as<TagTy, caldera::ImageTag>)
                device.bindImageMemory(resource, memory, binding.offset);
            else
                device.bindBufferMemory(resource, memory, binding.offset);
        }
    }
}

namespace caldera
{
    GraphAllocator::GraphAllocator(
        VmaAllocator const allocator,
        FamilyNumbers const families)
    noexcept :
        m_allocator(allocator),
        m_families(families),
        m_device(retrieve_device(allocator))
    {}

    void GraphAllocator::allocate(GraphIR const& ir, RenderGraph& graph)
    {
        assert(!RenderGraph::AllocatorAttorney::is_allocated(graph));
        auto const& resources = GraphIR::ResourcesAttorney::get_resources(ir);

        detail::AllocatedGraph allocated {};

        allocated.images = ::create_images(m_device, m_families, resources.transientImages);
        allocated.buffers = ::create_buffers(m_device, m_families, resources.transientBuffers);
        allocated.memory = ::allocate_heaps(m_allocator, resources.heaps);

        ::bind_memory<ImageTag>(m_device, allocated.memory,
            allocated.images, resources.transientImages);

        ::bind_memory<BufferTag>(m_device, allocated.memory,
            allocated.buffers, resources.transientBuffers);

        RenderGraph::AllocatorAttorney::push_allocated(graph, std::move(allocated));
    }

    void GraphAllocator::deallocate(RenderGraph& graph)
    {
        assert(RenderGraph::AllocatorAttorney::is_allocated(graph));

        auto const allocated = RenderGraph::AllocatorAttorney::extract_allocated(graph);

        for (auto const resource : allocated.images)
            m_device.destroyImage(resource);

        for (auto const resource : allocated.buffers)
            m_device.destroyBuffer(resource);

        for (auto const& heap : allocated.memory)
            vmaFreeMemory(m_allocator, heap.allocation);
    }
}