#pragma once

#include <cstdint>
#include <vector>
#include <span>
#include <unordered_map>
#include <functional>

#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>

#include <caldera/fwd.h>
#include <caldera/resource_id.hpp>

namespace caldera
{
    enum class MappingPolicy
    {
        persistent_coherent,
        persistent_noncoherent,
        dynamic_noncoherent,
        unmappable
    };

    enum class FamilyType
    {
        graphics,
        transfer,
        compute
    };

    struct FamilyNumbers
    {
        uint32_t graphics;
        uint32_t transfer;
        uint32_t compute;

        [[nodiscard]] uint32_t get(FamilyType type) const noexcept;
    };

    enum class ImageUsage
    {
        transfer,
        sampled,
        storage,
        color_attachment,
        depth_stencil_attachment,
        input_attachment,
        resolve_attachment,
        host
    };

    enum class BufferUsage
    {
        transfer,
        vertices,
        indices,
        uniform,
        storage,
        host
    };

    struct ImageDescription
    {
        vk::ImageCreateFlags flags;

        vk::ImageType type;
        vk::Format format;
        vk::Extent3D extent;
        uint32_t mipLevels;
        uint32_t arrayLayers;
        vk::SampleCountFlagBits samples;
        vk::ImageTiling tiling;
    };

    struct BufferDescription
    {
        vk::BufferCreateFlags flags;
        vk::DeviceSize size;

        bool hostVisible;
        bool constantlyMapped;
    };

    struct PassDependency
    {
        uint32_t otherPass;
        vk::PipelineStageFlags2 srcStages;
        vk::PipelineStageFlags2 dstStages;
    };

    struct ImageLastState
    {
        vk::PipelineStageFlags2 stages;
        vk::AccessFlags2 access;
        vk::ImageLayout layout;
        uint32_t familyIdx;
    };

    struct BufferLastState
    {
        vk::PipelineStageFlags2 stages;
        vk::AccessFlags2 access;
        uint32_t familyIdx;
    };
}

namespace caldera::detail
{
    enum class HazardType
    {
        none,
        access,
        ownership
    };

    struct ImageTouch
    {
        vk::PipelineStageFlags2 stages;
        vk::AccessFlags2 access;
        vk::ImageLayout layout;

        FamilyType family;
        uint32_t passIdx;
    };

    struct BufferTouch
    {
        vk::PipelineStageFlags2 stages;
        vk::AccessFlags2 access;

        FamilyType family;
        uint32_t passIdx;
    };

    constexpr ImageTouch undefined_image_touch = {
        .stages = {},
        .access = {},
        .layout = vk::ImageLayout::eUndefined,
        .family = {},
        .passIdx = UINT32_MAX
    };

    constexpr BufferTouch undefined_buffer_touch = {
        .stages = {},
        .access = {},
        .family = {},
        .passIdx = UINT32_MAX
    };

    enum class AccessKind
    {
        read,
        write,
        read_write
    };

    struct ImageVersion
    {
        ImageTouch writing;
        std::vector<ImageTouch> readingList;
    };

    struct BufferVersion
    {
        BufferTouch writing;
        std::vector<BufferTouch> readingList;
    };

    using ImageVersionTable = std::unordered_map<ImageID, std::vector<ImageVersion>>;
    using BufferVersionTable = std::unordered_map<BufferID, std::vector<BufferVersion>>;

    struct ImageBarrier
    {
        vk::PipelineStageFlags2 srcStages;
        vk::AccessFlags2 srcAccess;
        vk::PipelineStageFlags2 dstStages;
        vk::AccessFlags2 dstAccess;
        vk::ImageLayout oldLayout;
        vk::ImageLayout newLayout;
        uint32_t srcFamilyIdx;
        uint32_t dstFamilyIdx;
        ImageID id;
        vk::ImageSubresourceRange subresourceRange = {};
    };

    struct BufferBarrier
    {
        vk::PipelineStageFlags2 srcStages;
        vk::AccessFlags2 srcAccess;
        vk::PipelineStageFlags2 dstStages;
        vk::AccessFlags2 dstAccess;
        uint32_t srcFamilyIdx;
        uint32_t dstFamilyIdx;
        BufferID id;
        vk::DeviceSize offset;
        vk::DeviceSize size;
    };

    struct ImageTrackedInfo
    {
        ImageTouch firstTouch;
        ImageTouch lastTouch;

        vk::ImageUsageFlags usages;
        vk::ImageAspectFlags aspect;
    };

    struct BufferTrackedInfo
    {
        BufferTouch firstTouch;
        BufferTouch lastTouch;

        vk::BufferUsageFlags usages;

        uint8_t mappedAccessPercentage;
        bool hasRandomMappedAccess;
        bool hasMappedReading;
    };

    using ImageTrackingTable = std::unordered_map<ImageID, ImageTrackedInfo>;
    using BufferTrackingTable = std::unordered_map<BufferID, BufferTrackedInfo>;

    struct CommandFamily
    {
        std::vector<vk::CommandBuffer> buffers;
        uint32_t activeCount;
    };

    struct PassDeclaration
    {
        std::string name;
        FamilyType family;

        std::function<void(vk::CommandBuffer, ResourceMap const&)> callback;
    };

    struct GraphAdjacencyList
    {
        std::vector<std::vector<uint32_t>> passChildren;
        std::vector<std::vector<PassDependency>> passParents;
    };

    struct GraphResources
    {
        struct AllocatedImage
        {
            vk::Image handle;
            uint32_t heapIdx;
        };

        struct AllocatedBuffer
        {
            vk::Buffer handle;
            uint32_t heapIdx;
        };

        struct AllocatedHeap
        {
            VmaAllocation handle;
            vk::DeviceMemory memory;
            void* mapped;
        };

        std::vector<AllocatedImage> transientImages;
        std::vector<AllocatedBuffer> transientBuffers;

        std::vector<AllocatedHeap> imageHeaps;
        std::vector<AllocatedHeap> bufferHeaps;
    };

    struct CompiledPass
    {
        std::string name;
        uint32_t familyIdx;

        std::function<void(vk::CommandBuffer, ResourceMap const&)> callback;

        uint32_t imagePreBarrierOffset;
        uint32_t imagePreBarrierSize;

        uint32_t bufferPreBarrierOffset;
        uint32_t bufferPreBarrierSize;

        uint32_t imagePostBarrierOffset;
        uint32_t imagePostBarrierSize;

        uint32_t bufferPostBarrierOffset;
        uint32_t bufferPostBarrierSize;

        uint32_t allowedImageOffset;
        uint32_t allowedImageSize;

        uint32_t allowedBufferOffset;
        uint32_t allowedBufferSize;

        uint32_t passDependencyOffset;
        uint32_t passDependencySize;
    };

    struct CompiledGraph
    {
        std::vector<ImageBarrier> imageBarriers;
        std::vector<BufferBarrier> bufferBarriers;

        std::vector<ImageID> allowedImages;
        std::vector<BufferID> allowedBuffers;

        std::vector<PassDependency> passDependencies;

        ImageTrackingTable trackedImages;
        BufferTrackingTable trackedBuffer;

        std::vector<CompiledPass> compiledPasses;
    };
}

namespace caldera
{
    struct GraphIR
    {
        std::vector<ImageDescription> transientImages;
        std::vector<BufferDescription> transientBuffers;

        std::vector<detail::PassDeclaration> passes;

        detail::ImageVersionTable imageVersions;
        detail::BufferVersionTable bufferVersions;
    };

    struct SubmittableGraph
    {
        struct Pass
        {
            uint32_t familyIdx;
            vk::CommandBuffer commandBuffer;
            std::span<PassDependency> passDependencies;
        };

        std::span<Pass> passes;
    };

    struct ExecutionContext
    {
        // FamilyNumbers families;
        ResourceMap *resourceMap;
        ResourceStateManager *stateManager;
        CommandBufferDistributor *distributor;
    };

    struct ExecutionMemory
    {
        std::vector<vk::ImageMemoryBarrier2> imageBarriers;
        std::vector<vk::BufferMemoryBarrier2> bufferBarriers;

        std::vector<PassDependency> passDependencies;
        std::vector<SubmittableGraph::Pass> submittablePasses;
    };
}