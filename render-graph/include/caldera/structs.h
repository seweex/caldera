#pragma once

#include <cstdint>
#include <vulkan/vulkan.hpp>

#include "resource_id.h"

namespace caldera
{
    enum class FamilyType : uint16_t
    {
        unknown,
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
    };

    struct ExecutionContext
    {
        FamilyNumbers families;

        ResourceMap* resourceMap;
        ResourceStateManager* stateManager;

        SemaphoreMap* semaphoreMap;
        CommandBufferDistributor* distributor;
    };

    struct PassDependency
    {
        uint32_t otherPassIdx;
        vk::PipelineStageFlags2 stages;
    };

    struct OwnershipReleaseRequirements
    {
        vk::PipelineStageFlags2 srcStage;
        vk::AccessFlags2 srcAccess;

        FamilyType srcFamily;
        FamilyType dstFamily;
    };
}
