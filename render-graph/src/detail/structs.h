#pragma once

#include <cstdint>
#include <vector>
#include <functional>
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>

#include <caldera/structs.h>

namespace caldera::detail
{
    struct ResourceTouch
    {
        vk::PipelineStageFlags2 stages;
        vk::AccessFlags2 access;

        vk::ImageLayout layout;
        FamilyType family;
    };

    enum class AccessKind
    {
        read,
        write,
        read_write
    };

    struct ImageTouch
    {
        AccessKind access;
        ImageUsage usage;
        FamilyType family;
    };

    struct BufferTouch
    {
        AccessKind access;
        BufferUsage usage;
        FamilyType family;
    };

    struct ImageVersionDesc
    {
        struct Toucher
        {
            FamilyType family;
            ImageUsage usage;
            uint32_t passIdx;

            struct Comparator {
                [[nodiscard]] bool operator()(Toucher const& l, Toucher const& r) const noexcept;
            };
        };

        Toucher writing;
        std::vector<Toucher> readings;
    };

    struct BufferVersionDesc
    {
        struct Toucher
        {
            FamilyType family;
            BufferUsage usage;
            uint32_t passIdx;

            struct Comparator {
                [[nodiscard]] bool operator()(Toucher const& l, Toucher const& r) const noexcept;
            };
        };

        Toucher writing;
        std::vector<Toucher> readings;
    };

    /* Graph Descriptions */

    struct PassDeclaration
    {
        std::string name;
        FamilyType family;

        std::function<void(ResourceMap const&, vk::CommandBuffer)> callback;
    };

    struct ImageAccess
    {
        ImageVersion version;
        ImageUsage usage;
        AccessKind access;
    };

    struct BufferAccess
    {
        BufferVersion version;
        BufferUsage usage;
        AccessKind access;
    };

    struct MemoryHeapInfo
    {
        vk::DeviceSize size;
        vk::DeviceSize alignment;

        uint32_t memoryTypeBit;
    };

    struct MemoryHeapBinding
    {
        uint32_t heapIdx;
        vk::DeviceSize offset;
    };

    struct ImageTrackedInfo
    {
        vk::ImageUsageFlags usages;
        vk::ImageAspectFlags aspects;

        uint32_t firstPass;

        ResourceTouch firstTouch;
        ResourceTouch lastTouch;
    };

    struct BufferTrackedInfo
    {
        vk::BufferUsageFlags usages;

        uint32_t firstPass;

        ResourceTouch firstTouch;
        ResourceTouch lastTouch;
    };

    struct ImageCreateInfo
    {
        vk::ImageCreateFlags flags;
        vk::ImageUsageFlags usage;

        vk::ImageType type;
        vk::Format format;
        vk::Extent3D extent;
        uint32_t mipLevels;
        uint32_t arrayLayers;
        vk::SampleCountFlagBits samples;
        vk::ImageTiling tiling;
        FamilyType family;

        MemoryHeapBinding binding;
    };

    struct BufferCreateInfo
    {
        vk::BufferCreateFlags flags;
        vk::BufferUsageFlags usage;

        vk::DeviceSize size;
        FamilyType family;

        MemoryHeapBinding binding;
    };

    struct GraphExecutionIR
    {
        std::vector<PassDeclaration> passes;
        std::vector<uint32_t> passOrder;
    };

    struct GraphResourceIR
    {
        std::unordered_map<ImageID, std::vector<ImageVersionDesc>> imageVersions;
        std::unordered_map<BufferID, std::vector<BufferVersionDesc>> bufferVersions;

        std::unordered_map<ImageID, ImageTrackedInfo> imageTrackedInfos;
        std::unordered_map<BufferID, BufferTrackedInfo> bufferTrackedInfos;

        std::vector<MemoryHeapInfo> heaps;

        std::vector<ImageCreateInfo> transientImages;
        std::vector<BufferCreateInfo> transientBuffers;
    };

    /* Graph Barriers */

    struct ImageBarrier
    {
        ImageID image;

        vk::PipelineStageFlags2 srcStage;
        vk::AccessFlags2 srcAccess;

        vk::PipelineStageFlags2 dstStage;
        vk::AccessFlags2 dstAccess;

        vk::ImageLayout oldLayout;
        vk::ImageLayout newLayout;

        FamilyType srcFamily;
        FamilyType dstFamily;

        vk::ImageAspectFlags aspect;
    };

    struct BufferBarrier
    {
        BufferID buffer;

        vk::PipelineStageFlags2 srcStage;
        vk::AccessFlags2 srcAccess;

        vk::PipelineStageFlags2 dstStage;
        vk::AccessFlags2 dstAccess;

        FamilyType srcFamily;
        FamilyType dstFamily;
    };

    struct PassBarriers
    {
        std::vector<ImageBarrier> imagePreBarriers;
        std::vector<BufferBarrier> bufferPreBarriers;

        std::vector<ImageBarrier> imagePostBarriers;
        std::vector<BufferBarrier> bufferPostBarriers;

        std::vector<PassDependency> signalDeps;
        std::vector<PassDependency> waitDeps;
    };

    struct CompiledGraph
    {
        std::unordered_map<ImageID, ImageTrackedInfo> imageTrackedInfos;
        std::unordered_map<BufferID, BufferTrackedInfo> bufferTrackedInfos;

        std::vector<PassDeclaration> passes;
        std::vector<PassBarriers> barriers;
    };

    /* Graph Resources */

    struct MemoryHeap
    {
        VmaAllocation allocation;
        vk::DeviceMemory memory;
    };

    struct AllocatedGraph
    {
        std::vector<MemoryHeap> memory;

        std::vector<vk::Image> images;
        std::vector<vk::Buffer> buffers;
    };
}