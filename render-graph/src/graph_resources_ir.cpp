#include <ranges>
#include <vulkan/vulkan_format_traits.hpp>

#include <caldera/graph_ir.h>
#include <caldera/structs.h>

#include "detail/command_buffer_distributor.h"
#include "detail/graph_ir.h"
#include "detail/usage_translator.h"

namespace
{
    [[nodiscard]] vk::ImageAspectFlags
    get_aspect_flags(vk::Format const format) noexcept
    {
        vk::ImageAspectFlags result = {};

        if (vk::hasRedComponent(format) ||
            vk::hasGreenComponent(format) ||
            vk::hasBlueComponent(format) ||
            vk::hasAlphaComponent(format))
        {
            result |= vk::ImageAspectFlagBits::eColor;
        }

        if (vk::hasDepthComponent(format))
            result |= vk::ImageAspectFlagBits::eDepth;

        if (vk::hasStencilComponent(format))
            result |= vk::ImageAspectFlagBits::eStencil;

        return result;
    }

    void append_tracked_infos(
        auto& tracked,
        auto const& table,
        auto const descriptions)
    {
        using namespace caldera;

        detail::UsageTranslator translator = {};

        using IDTy = std::remove_cvref_t<decltype(table)>::key_type;
        tracked.reserve(tracked.size() + table.size());

        for (auto const& [resourceID, versionList] : table)
        {
            auto& trackedInfo = tracked[resourceID];

            if constexpr (std::same_as<IDTy, ImageID>)
                if (descriptions)
                {
                    assert(resourceID.is_transient);

                    auto const& desc = descriptions->at(resourceID.index);
                    trackedInfo.aspects = get_aspect_flags(desc.format);
                }

            // if (!versionList[0].readings.empty())
            //     trackedInfo.firstFamily = versionList[0].readings[0].family;
            // else
            //     trackedInfo.firstFamily = versionList[1].writing.family;
            //
            // if (!versionList.back().readings.empty())
            //     trackedInfo.lastFamily = versionList.back().readings.back().family;
            // else
            //     trackedInfo.lastFamily = versionList.back().writing.family;

            if (!versionList[0].readings.empty())
            {
                auto const& toucher = versionList[0].readings[0];

                trackedInfo.firstPass = toucher.passIdx;
                trackedInfo.firstTouch = translator.translate_usage(
                    toucher.family, toucher.usage, detail::AccessKind::read);
            }
            else {
                auto const& toucher = versionList[1].writing;

                trackedInfo.firstPass = toucher.passIdx;
                trackedInfo.firstTouch = translator.translate_usage(
                    toucher.family, toucher.usage, detail::AccessKind::write);
            }

            if (!versionList.back().readings.empty())
            {
                auto const& toucher = versionList.back().readings.back();

                trackedInfo.lastTouch = translator.translate_usage(
                    toucher.family, toucher.usage, detail::AccessKind::read);
            }
            else {
                auto const& toucher = versionList.back().writing;

                trackedInfo.lastTouch = translator.translate_usage(
                    toucher.family, toucher.usage, detail::AccessKind::write);
            }

            for (auto const& [writing, readings] : versionList)
            {
                trackedInfo.usages |= translator.translate_flags(writing.usage, detail::AccessKind::write);

                for (auto const& reading : readings)
                    trackedInfo.usages |= translator.translate_flags(reading.usage, detail::AccessKind::read);
            }
        }
    }

    [[nodiscard]] caldera::detail::ImageCreateInfo
    form_create_info(
        caldera::ImageDescription const& description,
        caldera::detail::ImageTrackedInfo const& trackedInfo)
    {
        return caldera::detail::ImageCreateInfo
        {
            .flags = description.flags,
            .usage = trackedInfo.usages,
            .type = description.type,
            .format = description.format,
            .extent = description.extent,
            .mipLevels = description.mipLevels,
            .arrayLayers = description.arrayLayers,
            .samples = description.samples,
            .tiling = description.tiling,
            .family = trackedInfo.firstTouch.family
        };
    }

    [[nodiscard]] caldera::detail::BufferCreateInfo
    form_create_info(
        caldera::BufferDescription const& description,
        caldera::detail::BufferTrackedInfo const& trackedInfo)
    {
        return caldera::detail::BufferCreateInfo
        {
            .flags = description.flags,
            .usage = trackedInfo.usages,
            .size = description.size,
            .family = trackedInfo.firstTouch.family
        };
    }

    template <class CreateInfoTy>
    [[nodiscard]] std::vector<CreateInfoTy>
    make_create_infos(
        auto const& descriptions,
        auto const& trackedInfos)
    {
        std::vector<CreateInfoTy> result;
        result.reserve(descriptions.size());

        for (uint32_t resourceIdx = 0; resourceIdx < descriptions.size(); ++resourceIdx)
        {
            using ID = std::remove_cvref_t<decltype(trackedInfos)>::key_type;

            ID const resourceID{ 1, resourceIdx };

            auto const& desc = descriptions[resourceIdx];
            auto const& trackedInfo = trackedInfos.at(resourceID);

            result.push_back(form_create_info(desc, trackedInfo));
        }

        return result;
    }

    [[nodiscard]] vk::MemoryRequirements2 get_resource_memory_requirements(
        vk::Device const device,
        caldera::detail::ImageCreateInfo const& createInfo)
    {
        vk::ImageCreateInfo const vkInfo {
            createInfo.flags,
            createInfo.type,
            createInfo.format,
            createInfo.extent,
            createInfo.mipLevels,
            createInfo.arrayLayers,
            createInfo.samples,
            createInfo.tiling,
            createInfo.usage,
            vk::SharingMode::eExclusive,
            0u, nullptr
        };

        vk::DeviceImageMemoryRequirements const requirementsInfo { &vkInfo };
        return device.getImageMemoryRequirements(requirementsInfo);
    }

    [[nodiscard]] vk::MemoryRequirements2 get_resource_memory_requirements(
        vk::Device const device,
        caldera::detail::BufferCreateInfo const& createInfo)
    {
        vk::BufferCreateInfo const vkInfo {
            createInfo.flags,
            createInfo.size,
            createInfo.usage,
            vk::SharingMode::eExclusive,
            0u, nullptr
        };

        vk::DeviceBufferMemoryRequirements const requirementsInfo { &vkInfo };
        return device.getBufferMemoryRequirements(requirementsInfo);
    }

    [[nodiscard]] std::vector<vk::MemoryRequirements2>
    get_memory_requirements(
        vk::Device const& device,
        auto const& createInfos)
    {
        std::vector<vk::MemoryRequirements2> result;
        result.reserve(createInfos.size());

        for (auto const& info : createInfos)
            result.push_back(get_resource_memory_requirements(device, info));

        return result;
    }

    void append_with_bound_heaps(
        vk::Device const device,
        auto& createInfos,
        auto& heaps)
    {
        auto const memoryReqs = get_memory_requirements(device, createInfos);
        heaps.reserve(heaps.size() + createInfos.size());

        for (uint32_t resourceIdx = 0; resourceIdx < createInfos.size(); ++resourceIdx)
        {
            auto const heapIdx = static_cast<uint32_t>(heaps.size());
            auto const& memoryReq = memoryReqs[resourceIdx];

            heaps.emplace_back(
                memoryReq.memoryRequirements.size,
                memoryReq.memoryRequirements.alignment,
                memoryReq.memoryRequirements.memoryTypeBits);

            auto& binding = createInfos[resourceIdx].binding;
            binding.heapIdx = heapIdx;
            binding.offset = 0;
        }
    }
}

namespace caldera
{
    void GraphIR::Factory::describe_resources(
        GraphIR &ir,
        vk::Device const device,
        std::vector<ImageDescription> const& transientImages,
        std::vector<BufferDescription> const& transientBuffers,
        std::unordered_map<ImageID, std::vector<detail::ImageVersionDesc>> const& imageVersions,
        std::unordered_map<BufferID, std::vector<detail::BufferVersionDesc>> const& bufferVersions)
    {
        auto& resources = ir.m_impl->resources;

        assert(transientImages.empty() || !imageVersions.empty());
        assert(transientBuffers.empty() || !bufferVersions.empty());

        resources.imageVersions = imageVersions;
        resources.bufferVersions = bufferVersions;

        ::append_tracked_infos(resources.imageTrackedInfos, imageVersions, &transientImages);
        ::append_tracked_infos(resources.bufferTrackedInfos, bufferVersions, nullptr);

        resources.transientImages = ::make_create_infos
            <detail::ImageCreateInfo>(transientImages, resources.imageTrackedInfos);

        resources.transientBuffers = ::make_create_infos
            <detail::BufferCreateInfo>(transientBuffers, resources.bufferTrackedInfos);

        ::append_with_bound_heaps(device, resources.transientImages, resources.heaps);
        ::append_with_bound_heaps(device, resources.transientBuffers, resources.heaps);
    }

    detail::GraphResourceIR const&
    GraphIR::ResourcesAttorney::get_resources(GraphIR const &ir) noexcept
    {
        return ir.m_impl->resources;
    }
}
