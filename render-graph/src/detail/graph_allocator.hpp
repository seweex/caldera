#pragma once

#include <expected>
#include <vector>
#include <concepts>
#include <cstdint>
#include <cassert>
#include <functional>
#include <variant>

#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>

#include <caldera/structs.hpp>
#include <caldera/detail/graph_allocator.hpp>

namespace caldera::detail
{
    struct AllocationDescription
    {
        vk::DeviceSize size;
        vk::DeviceSize alignment;

        uint32_t memoryTypeBits;
        VmaAllocationCreateFlags createFlags;
        VkMemoryPropertyFlags requiredFlags;
        VkMemoryPropertyFlags preferredFlags;

        std::variant<std::monostate, vk::Image, vk::Buffer> dedicatedTarget;
    };

    struct ResourceBinding
    {
        uint32_t heapIdx;
        vk::DeviceSize offset;
    };

    struct DescribedAllocations
    {
        std::vector<AllocationDescription> descriptionList;
        std::vector<ResourceBinding> resourceBindings;
    };

    /* * * * */

    [[nodiscard]] inline MappingPolicy
    choose_mapping_policy(BufferTrackedInfo const& info) noexcept
    {
        uint8_t constexpr high_mapped_accessed_percentage_threshold = 60;
        uint8_t constexpr medium_mapped_accessed_percentage_threshold = 30;

        if (info.mappedAccessPercentage > high_mapped_accessed_percentage_threshold)
            return MappingPolicy::persistent_coherent;

        if (info.mappedAccessPercentage > medium_mapped_accessed_percentage_threshold)
            return MappingPolicy::persistent_noncoherent;

        if (info.mappedAccessPercentage > 0)
            return MappingPolicy::dynamic_noncoherent;

        return MappingPolicy::unmappable;
    }

    template <class TagTy>
    [[nodiscard]] DescribedAllocations describe_allocations(
        vk::Device const device,
        auto const& trackingTable,
        auto const& allocatedResources)
    {
        auto const resourceCount = allocatedResources.size();

        DescribedAllocations result;
        result.descriptionList.reserve(resourceCount);
        result.resourceBindings.reserve(resourceCount);

        auto const getRequirements = [&] <typename Ty> (Ty const handle)
        {
            if constexpr (std::same_as<Ty, vk::Image>)
            {
                vk::ImageMemoryRequirementsInfo2 const request { handle };

                return device.getImageMemoryRequirements2<
                    vk::MemoryRequirements2, vk::MemoryDedicatedRequirements>(request);
            }
            else {
                vk::BufferMemoryRequirementsInfo2 const request { handle };

                return device.getBufferMemoryRequirements2<
                    vk::MemoryRequirements2, vk::MemoryDedicatedRequirements>(request);
            }
        };

        for (uint32_t resourceIdx = 0; resourceIdx < resourceCount; ++resourceIdx)
        {
            BasicResourceID<TagTy> const resourceID {
                .isTransient = 1,
                .index = resourceIdx
            };

            auto const handle = allocatedResources[resourceIdx];
            auto const& trackedInfo = trackingTable.at(resourceID);

            auto const [requirements, dedicated] = getRequirements(handle);

            AllocationDescription description {
                .size = requirements.memoryRequirements.size,
                .alignment = requirements.memoryRequirements.alignment,
                .memoryTypeBits = requirements.memoryRequirements.memoryTypeBits,
                .createFlags = {},
                .requiredFlags = {},
                .preferredFlags = {},
                .dedicatedTarget = {}
            };

            if constexpr (std::same_as<TagTy, BufferTag>)
            {
                auto const mappedPolicy = choose_mapping_policy(trackedInfo);

                if (mappedPolicy != MappingPolicy::unmappable)
                {
                    bool const reads = trackedInfo.hasRandomMappedAccess || trackedInfo.hasMappedReading;

                    description.createFlags |= reads
                        ? VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT
                        : VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

                    if (mappedPolicy == MappingPolicy::persistent_coherent ||
                        mappedPolicy == MappingPolicy::persistent_noncoherent)
                    {
                        description.createFlags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
                    }

                    if (mappedPolicy == MappingPolicy::persistent_coherent)
                        description.requiredFlags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

                    if (trackedInfo.hasMappedReading)
                        description.preferredFlags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
                }
            }

            if (dedicated.prefersDedicatedAllocation || dedicated.requiresDedicatedAllocation) {
                description.createFlags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
                description.dedicatedTarget = handle;
            }

            auto const heapIdx = static_cast<uint32_t>(result.descriptionList.size());

            result.descriptionList.emplace_back(description);
            result.resourceBindings.emplace_back(heapIdx, 0);
        }

        return result;
    }

    /* * * * */

    [[nodiscard]] auto project_handle(auto const& handle) noexcept
    {
        if constexpr (requires{ handle.handle; })
            return handle.handle;
        else
            return handle;
    }

    void destroy_images(
        vk::Device const device,
        auto const& allocatedImages) noexcept
    {
        for (auto const& allocated : allocatedImages)
            device.destroyImage(project_handle(allocated));
    }

    void destroy_buffers(
        vk::Device const device,
        auto const& allocatedBuffers) noexcept
    {
        for (auto const& allocated : allocatedBuffers)
            device.destroyBuffer(project_handle(allocated));
    }

    void destroy_heaps(
        VmaAllocator const allocator,
        auto const& allocatedHeaps) noexcept
    {
        for (auto const& allocated : allocatedHeaps)
            vmaFreeMemory(allocator, project_handle(allocated));
    }

    /* * * * */

    [[nodiscard]] std::expected<std::vector<vk::Image>, vk::Result>
    create_images(
        vk::Device const device,
        FamilyNumbers const& families,
        auto const& descriptionList,
        auto const& trackingTable)
    {
        auto const resourceCount = descriptionList.size();

        std::vector<vk::Image> result;
        result.reserve(resourceCount);

        for (uint32_t resourceIdx = 0; resourceIdx < resourceCount; ++resourceIdx)
        {
            ImageID const id {
                .isTransient = 1,
                .index = resourceIdx
            };

            auto const& description = descriptionList[resourceIdx];
            auto const& trackedInfo = trackingTable.at(id);

            auto const family = families.get(trackedInfo.firstTouch.family);

            vk::ImageCreateInfo const createInfo {
                description.flags,
                description.type,
                description.format,
                description.extent,
                description.mipLevels,
                description.arrayLayers,
                description.samples,
                description.tiling,
                trackedInfo.usages,
                vk::SharingMode::eExclusive,
                1u, &family,
                vk::ImageLayout::eUndefined
            };

            vk::Image image;

            if (auto const rCode = device.createImage(&createInfo, nullptr, &image);
                rCode < vk::Result::eSuccess)
            {
                destroy_images(device, result);
                return std::unexpected(rCode);
            }

            result.emplace_back(image);
        }

        return result;
    }

    [[nodiscard]] std::expected<std::vector<vk::Buffer>, vk::Result>
    create_buffers(
        vk::Device const device,
        FamilyNumbers const& families,
        auto const& descriptionList,
        auto const& trackingTable)
    {
        auto const resourceCount = descriptionList.size();

        std::vector<vk::Buffer> result;
        result.reserve(resourceCount);

        for (uint32_t resourceIdx = 0; resourceIdx < resourceCount; ++resourceIdx)
        {
            BufferID const id {
                .isTransient = 1,
                .index = resourceIdx
            };

            auto const& description = descriptionList[resourceIdx];
            auto const& trackedInfo = trackingTable.at(id);

            auto const family = families.get(trackedInfo.firstTouch.family);

            vk::BufferCreateInfo const createInfo {
                description.flags,
                description.size,
                trackedInfo.usages,
                vk::SharingMode::eExclusive,
                1u, &family
            };

            vk::Buffer buffer;

            if (auto const rCode = device.createBuffer(&createInfo, nullptr, &buffer);
                rCode < vk::Result::eSuccess)
            {
                destroy_buffers(device, result);
                return std::unexpected(rCode);
            }

            result.emplace_back(buffer);
        }

        return result;
    }

    template <class TagTy>
    [[nodiscard]] std::expected<std::vector<VmaAllocation>, vk::Result>
    create_allocations(
        VmaAllocator const allocator,
        auto const& descriptionList)
    {
        std::vector<VmaAllocation> result;
        result.reserve(descriptionList.size());

        for (uint32_t allocationIdx = 0; allocationIdx < descriptionList.size(); ++allocationIdx)
        {
            auto const& desc = descriptionList[allocationIdx];

            VkMemoryRequirements const requirementsInfo {
                .size = desc.size,
                .alignment = desc.alignment,
                .memoryTypeBits = desc.memoryTypeBits
            };

            VmaAllocationCreateInfo const createInfo {
                .flags = desc.createFlags,
                .usage = VMA_MEMORY_USAGE_AUTO,
                .requiredFlags = desc.requiredFlags,
                .preferredFlags = desc.preferredFlags,
                .memoryTypeBits = desc.memoryTypeBits,
                .pool = VK_NULL_HANDLE,
                .pUserData = nullptr,
                .priority = 1.f
            };

            VmaAllocation allocation;
            VkResult rCode;

            if (createInfo.flags & VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT)
            {
                if constexpr (std::same_as<TagTy, ImageTag>)
                    rCode = vmaAllocateMemoryForImage(
                        allocator, std::get<vk::Image>(desc.dedicatedTarget), &createInfo, &allocation, nullptr);
                else
                    rCode = vmaAllocateMemoryForBuffer(
                        allocator, std::get<vk::Buffer>(desc.dedicatedTarget), &createInfo, &allocation, nullptr);
            }
            else {
                rCode = vmaAllocateMemory(allocator, &requirementsInfo, &createInfo, &allocation, nullptr);
            }

            if (rCode < VK_SUCCESS)
            {
                destroy_heaps(allocator, result);
                return std::unexpected(vk::Result{ rCode });
            }

            result.emplace_back(allocation);
        }

        return result;
    }

    template <class TagTy>
    [[nodiscard]] vk::Result bind_allocations(
        VmaAllocator const allocator,
        auto const& resourceBindings,
        auto const& allocations,
        auto const& allocatedResources)
    {
        for (uint32_t resourceIdx = 0; resourceIdx < resourceBindings.size(); ++resourceIdx)
        {
            auto const binding = resourceBindings[resourceIdx];

            auto const resourceHandle = allocatedResources[resourceIdx];
            auto const allocationHandle = allocations[binding.heapIdx];

            VkResult rCode;

            if constexpr (std::same_as<TagTy, ImageTag>)
                rCode = vmaBindImageMemory2(allocator, allocationHandle, binding.offset, resourceHandle, nullptr);
            else
                rCode = vmaBindBufferMemory2(allocator, allocationHandle, binding.offset, resourceHandle, nullptr);

            if (rCode < VK_SUCCESS)
                return static_cast<vk::Result>(rCode);
        }

        return vk::Result::eSuccess;
    }

    /* * * * */

    void append_with_translated_resources(
        VmaAllocator const allocator,
        auto& targetResourceList,
        auto& targetHeapList,
        auto const& allocatedResources,
        auto const& allocatedHeaps,
        auto const& allocatedHeapDescriptions)
    {
        auto const resourceCount = allocatedResources.size();

        assert(targetResourceList.capacity() >= resourceCount);
        assert(targetHeapList.capacity() >= resourceCount);

        auto const translateAllocation = [&] (VmaAllocation const allocation)
        {
            VmaAllocationInfo info;
            vmaGetAllocationInfo(allocator, allocation, &info);

            return GraphResources::AllocatedHeap {
                .handle = allocation,
                .memory = info.deviceMemory,
                .mapped = info.pMappedData
            };
        };

        for (uint32_t resourceIdx = 0; resourceIdx < allocatedResources.size(); ++resourceIdx)
            targetResourceList.emplace_back(
                allocatedResources[resourceIdx],
                allocatedHeapDescriptions.resourceBindings[resourceIdx].heapIdx);

        for (uint32_t heapIdx = 0; heapIdx < allocatedHeaps.size(); ++heapIdx)
            targetHeapList.push_back(translateAllocation(allocatedHeaps[heapIdx]));
    }

    template <class TagTy>
    [[nodiscard]] vk::Result append_with_allocated(
        vk::Device const device,
        VmaAllocator const allocator,
        FamilyNumbers const& families,
        auto const& descriptionList,
        auto const& trackingTable,
        auto& targetResourceList,
        auto& targetHeapList)
    {
        auto const allocatedResources = std::invoke([&] {
            if constexpr (std::same_as<TagTy, ImageTag>)
                return create_images(device, families, descriptionList, trackingTable);
            else
                return create_buffers(device, families, descriptionList, trackingTable);
        });

        if (!allocatedResources)
            return allocatedResources.error();

        auto const cleanupResources = [&]
        {
            if constexpr (std::same_as<TagTy, ImageTag>)
                destroy_images(device, *allocatedResources);
            else
                destroy_buffers(device, *allocatedResources);
        };

        auto const allocationDescriptions = describe_allocations<TagTy>(device, trackingTable, *allocatedResources);
        auto const allocatedHeaps = create_allocations<TagTy>(allocator, allocationDescriptions.descriptionList);

        if (!allocatedHeaps) {
            cleanupResources();
            return allocatedHeaps.error();
        }

        auto const rCode = bind_allocations<TagTy>(allocator,
            allocationDescriptions.resourceBindings, *allocatedHeaps, *allocatedResources);

        if (rCode < vk::Result::eSuccess) {
            cleanupResources();
            destroy_heaps(allocator, *allocatedHeaps);
            return rCode;
        }

        append_with_translated_resources(allocator, targetResourceList, targetHeapList,
            *allocatedResources, *allocatedHeaps, allocationDescriptions);

        return vk::Result::eSuccess;
    }
}