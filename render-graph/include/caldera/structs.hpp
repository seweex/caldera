#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <string>
#include <span>
#include <unordered_map>
#include <functional>

#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>

#include <caldera/fwd.hpp>
#include <caldera/resource_id.hpp>

namespace caldera
{
    /// @brief Policy of mapping of a memory block
    enum class MappingPolicy
    {
        /// Requires persistent mapping for allocation and prefers coherent memory
        persistent_coherent,

        /// Requires persistent mapping for allocation and prefers non-coherent memory
        persistent_noncoherent,

        /// Requires mapping for allocation and prefers non-persistent mapping and non-coherent memory
        dynamic_noncoherent,

        /// Doesn't require any mapping
        unmappable
    };

    /// @brief Type of queue family
    enum class FamilyType
    {
        /// @brief Queue with graphics command support
        graphics,

        /// @brief Queue with transfer command support
        transfer,

        /// @brief Queue with compute command support
        compute
    };

    /// @brief Queue family indices for each type of queue family
    struct FamilyNumbers
    {
        uint32_t graphics;
        uint32_t transfer;
        uint32_t compute;

        /**
         * @brief Returns queue family index of the required queue family type
         *
         * @param type Type of family to get index of
         * @return Index of required queue family type
         */
        [[nodiscard]] uint32_t get(FamilyType type) const noexcept;
    };

    /// @brief Modes of image usage
    enum class ImageUsage
    {
        /// @brief Image is used as transfer source or destination
        transfer,

        /// @brief Image is used as a sampled image
        sampled,

        /// @brief Image is used as a shader storage image
        storage,

        /// @brief Image is a color attachment
        color_attachment,

        /// @brief Image is a depth or/and stencil attachment
        depth_stencil_attachment,

        /// @brief Image is used as an input attachment
        input_attachment,

        /// @brief Image is used as an resolve attachment
        resolve_attachment,

        /// @brief Image is used by host
        host
    };

    /// @brief Modes of buffer usage
    enum class BufferUsage
    {
        /// @brief Buffer is a transfer source or destination
        transfer,

        /// @brief Buffer contains vertices
        vertices,

        /// @brief Buffer contains vertex indices
        indices,

        /// @brief Buffer is used as a uniform buffer
        uniform,

        /// @brief Buffer is used as a shader storage
        storage,

        /// @brief Buffer is used by host
        host
    };

    /// @brief Description of a transient image resource
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

    /// @brief Description of a transient buffer resource
    struct BufferDescription
    {
        vk::BufferCreateFlags flags;
        vk::DeviceSize size;

        bool hostVisible;
        bool constantlyMapped;
    };

    /// @brief Description of pass dependency
    struct PassDependency
    {
        /**
         * @brief Execution order index of the pass the current one
         * waits for or signals to
         */
        uint32_t otherPass;

        /// Stages of the producer execution to be waited
        vk::PipelineStageFlags2 srcStages;

        /// Stages of the consumer execution to wait on
        vk::PipelineStageFlags2 dstStages;
    };

    /// @brief Last usage of an image
    struct ImageLastState
    {
        vk::PipelineStageFlags2 stages;
        vk::AccessFlagBits2 access;
        vk::ImageLayout layout;
        uint32_t familyIdx;
    };

    /// @brief Last usage of a buffer
    struct BufferLastState
    {
        vk::PipelineStageFlags2 stages;
        vk::AccessFlagBits2 access;
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
    /// @brief Description of the graph to compile
    struct GraphIR
    {
        std::vector<ImageDescription> transientImages;
        std::vector<BufferDescription> transientBuffers;

        std::vector<detail::PassDeclaration> passes;

        detail::ImageVersionTable imageVersions;
        detail::BufferVersionTable bufferVersions;
    };

    /// @brief Description of an executed render graph
    struct SubmittableGraph
    {
        /// @brief Description of a pass in an executed render graph
        struct Pass
        {
            /// @brief Queue family index the commandBuffer should be submitted to
            uint32_t familyIdx;

            /// @brief Written vulkan command buffer
            vk::CommandBuffer commandBuffer;

            /// @brief Semaphore dependencies of the pass
            std::span<PassDependency> passDependencies;
        };

        std::span<Pass> passes;
    };

    /**
     * @brief Context of execution
     * @note All pointers in this structure are non-owning and must
     * refer to valid managers
     */
    struct ExecutionContext
    {
        ResourceMap *resourceMap;
        ResourceStateManager *stateManager;
        CommandBufferDistributor *distributor;
    };

    /// @brief Execution cache memory to avoid dynamic allocations
    struct ExecutionMemory
    {
        std::vector<vk::ImageMemoryBarrier2> imageBarriers;
        std::vector<vk::BufferMemoryBarrier2> bufferBarriers;

        std::vector<PassDependency> passDependencies;
        std::vector<SubmittableGraph::Pass> submittablePasses;
    };
}