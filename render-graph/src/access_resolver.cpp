#include "detail/access_resolver.h"
#include <cassert>

namespace
{
    constexpr auto writing_masks =
        vk::AccessFlagBits2::eShaderWrite |
        vk::AccessFlagBits2::eShaderStorageWrite |
        vk::AccessFlagBits2::eColorAttachmentWrite |
        vk::AccessFlagBits2::eDepthStencilAttachmentWrite |
        vk::AccessFlagBits2::eTransferWrite |
        vk::AccessFlagBits2::eHostWrite |
        vk::AccessFlagBits2::eMemoryWrite |
        vk::AccessFlagBits2::eAccelerationStructureWriteKHR |
        vk::AccessFlagBits2::eTransformFeedbackWriteEXT |
        vk::AccessFlagBits2::eTransformFeedbackCounterWriteEXT;
}

namespace caldera::detail
{
    HazardType AccessResolver::has_hazards(
        ResourceTouch const& src,
        ResourceTouch const& dst) const noexcept
    {
        if (src.family != dst.family && src.family != FamilyType::unknown && src.family != FamilyType::unknown)
            return HazardType::ownership;

        auto const hasAccessHazards =
            static_cast<bool>((src.access | dst.access) & ::writing_masks);

        auto const layoutChanged = src.layout != dst.layout;

        return (hasAccessHazards || layoutChanged) ? HazardType::access : HazardType::none;
    }

    ImageBarrier AccessResolver::resolve_access_hazard(
        ImageID const image,
        ResourceTouch const& src,
        ResourceTouch const& dst) const noexcept
    {
        assert(src.family == dst.family ||
            src.family == FamilyType::unknown ||
            dst.family == FamilyType::unknown);

        return ImageBarrier {
            .image     = image,
            .srcStage  = src.stages,
            .srcAccess = src.access,
            .dstStage  = dst.stages,
            .dstAccess = dst.access,
            .oldLayout = src.layout,
            .newLayout = dst.layout,
            .srcFamily = FamilyType::unknown,
            .dstFamily = FamilyType::unknown
        };
    }

    BufferBarrier AccessResolver::resolve_access_hazard(
        BufferID const buffer,
        ResourceTouch const& src,
        ResourceTouch const& dst) const noexcept
    {
        assert(src.family == dst.family ||
            src.family == FamilyType::unknown ||
            dst.family == FamilyType::unknown);

        return BufferBarrier {
            .buffer    = buffer,
            .srcStage  = src.stages,
            .srcAccess = src.access,
            .dstStage  = dst.stages,
            .dstAccess = dst.access,
            .srcFamily = FamilyType::unknown,
            .dstFamily = FamilyType::unknown,
        };
    }

    std::array<ImageBarrier, 2> AccessResolver::resolve_ownership_hazard(
        ImageID const image,
        ResourceTouch const& src,
        ResourceTouch const& dst) const noexcept
    {
        assert(src.family != dst.family);

        ImageBarrier const release {
            .image     = image,
            .srcStage  = src.stages,
            .srcAccess = src.access,
            .dstStage  = vk::PipelineStageFlagBits2::eNone,
            .dstAccess = vk::AccessFlagBits2::eNone,
            .oldLayout = src.layout,
            .newLayout = dst.layout,
            .srcFamily = src.family,
            .dstFamily = dst.family
        };

        ImageBarrier const acquire {
            .image     = image,
            .srcStage  = vk::PipelineStageFlagBits2::eNone,
            .srcAccess = vk::AccessFlagBits2::eNone,
            .dstStage  = dst.stages,
            .dstAccess = dst.access,
            .oldLayout = src.layout,
            .newLayout = dst.layout,
            .srcFamily = src.family,
            .dstFamily = dst.family
        };

        return { release, acquire };
    }

    std::array<BufferBarrier, 2> AccessResolver::resolve_ownership_hazard(
        BufferID const buffer,
        ResourceTouch const& src,
        ResourceTouch const& dst) const noexcept
    {
        assert(src.family != dst.family);

        BufferBarrier const release {
            .buffer    = buffer,
            .srcStage  = src.stages,
            .srcAccess = src.access,
            .dstStage  = vk::PipelineStageFlagBits2::eNone,
            .dstAccess = vk::AccessFlagBits2::eNone,
            .srcFamily = src.family,
            .dstFamily = dst.family,
        };

        BufferBarrier const acquire {
            .buffer    = buffer,
            .srcStage  = vk::PipelineStageFlagBits2::eNone,
            .srcAccess = vk::AccessFlagBits2::eNone,
            .dstStage  = dst.stages,
            .dstAccess = dst.access,
            .srcFamily = src.family,
            .dstFamily = dst.family,
        };

        return { release, acquire };
    }
}