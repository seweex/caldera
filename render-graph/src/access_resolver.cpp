#include <cassert>
#include <vulkan/vulkan.hpp>

#include <caldera/detail/access_resolver.hpp>
#include <caldera/structs.hpp>

namespace {
constexpr auto writing_masks =
    vk::AccessFlagBits2::eShaderWrite | vk::AccessFlagBits2::eShaderStorageWrite |
    vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentWrite |
    vk::AccessFlagBits2::eTransferWrite | vk::AccessFlagBits2::eHostWrite | vk::AccessFlagBits2::eMemoryWrite |
    vk::AccessFlagBits2::eAccelerationStructureWriteKHR | vk::AccessFlagBits2::eTransformFeedbackWriteEXT |
    vk::AccessFlagBits2::eTransformFeedbackCounterWriteEXT;

template <class TagTy>
[[nodiscard]] caldera::detail::HazardType has_hazards(caldera::FamilyNumbers const &families, auto const &src,
                                                      auto const &dst) {
    auto const srcFamily = families.get(src.family);
    auto const dstFamily = families.get(dst.family);

    if (srcFamily != dstFamily)
        return caldera::detail::HazardType::ownership;

    auto const hasAccessHazards = static_cast<bool>((src.access | dst.access) & ::writing_masks);

    bool layoutChanged;

    if constexpr (std::same_as<TagTy, caldera::ImageTag>)
        layoutChanged = src.layout != dst.layout;
    else
        layoutChanged = false;

    return (hasAccessHazards || layoutChanged) ? caldera::detail::HazardType::access
                                               : caldera::detail::HazardType::none;
}
} // namespace

namespace caldera::detail {
HazardType AccessResolver::has_hazards(FamilyNumbers const &families, ImageTouch const &src,
                                       ImageTouch const &dst) const noexcept {
    return ::has_hazards<ImageTag>(families, src, dst);
}

HazardType AccessResolver::has_hazards(FamilyNumbers const &families, BufferTouch const &src,
                                       BufferTouch const &dst) const noexcept {
    return ::has_hazards<BufferTag>(families, src, dst);
}

ImageBarrier AccessResolver::resolve_access_hazard(ImageID const image, ImageTouch const &src, ImageTouch const &dst,
                                                   ImageTrackedInfo const &trackedInfo) const noexcept {
    return ImageBarrier{
        .srcStages = src.stages,
        .srcAccess = src.access,
        .dstStages = dst.stages,
        .dstAccess = dst.access,
        .oldLayout = src.layout,
        .newLayout = dst.layout,
        .srcFamilyIdx = vk::QueueFamilyIgnored,
        .dstFamilyIdx = vk::QueueFamilyIgnored,
        .id = image,
        .subresourceRange = {trackedInfo.aspect, 0, vk::RemainingMipLevels, 0, vk::RemainingArrayLayers}};
}

BufferBarrier AccessResolver::resolve_access_hazard(BufferID const buffer, BufferTouch const &src,
                                                    BufferTouch const &dst, BufferTrackedInfo const &) const noexcept {
    return BufferBarrier{.srcStages = src.stages,
                         .srcAccess = src.access,
                         .dstStages = dst.stages,
                         .dstAccess = dst.access,
                         .srcFamilyIdx = vk::QueueFamilyIgnored,
                         .dstFamilyIdx = vk::QueueFamilyIgnored,
                         .id = buffer,
                         .offset = 0,
                         .size = vk::WholeSize};
}

std::array<ImageBarrier, 2>
AccessResolver::resolve_ownership_hazard(ImageID const image, FamilyNumbers const &families, ImageTouch const &src,
                                         ImageTouch const &dst, ImageTrackedInfo const &trackedInfo) const noexcept {
    auto const srcFamily = families.get(src.family);
    auto const dstFamily = families.get(src.family);

    ImageBarrier const release{
        .srcStages = src.stages,
        .srcAccess = src.access,
        .dstStages = vk::PipelineStageFlagBits2::eNone,
        .dstAccess = vk::AccessFlagBits2::eNone,
        .oldLayout = src.layout,
        .newLayout = dst.layout,
        .srcFamilyIdx = srcFamily,
        .dstFamilyIdx = dstFamily,
        .id = image,
        .subresourceRange = {trackedInfo.aspect, 0, vk::RemainingMipLevels, 0, vk::RemainingArrayLayers}};

    ImageBarrier const acquire{
        .srcStages = vk::PipelineStageFlagBits2::eNone,
        .srcAccess = vk::AccessFlagBits2::eNone,
        .dstStages = dst.stages,
        .dstAccess = dst.access,
        .oldLayout = src.layout,
        .newLayout = dst.layout,
        .srcFamilyIdx = srcFamily,
        .dstFamilyIdx = dstFamily,
        .id = image,
        .subresourceRange = {trackedInfo.aspect, 0, vk::RemainingMipLevels, 0, vk::RemainingArrayLayers}};

    return {release, acquire};
}

std::array<BufferBarrier, 2> AccessResolver::resolve_ownership_hazard(BufferID const buffer,
                                                                      FamilyNumbers const &families,
                                                                      BufferTouch const &src, BufferTouch const &dst,
                                                                      BufferTrackedInfo const &) const noexcept {
    auto const srcFamily = families.get(src.family);
    auto const dstFamily = families.get(src.family);

    BufferBarrier const release{.srcStages = src.stages,
                                .srcAccess = src.access,
                                .dstStages = vk::PipelineStageFlagBits2::eNone,
                                .dstAccess = vk::AccessFlagBits2::eNone,
                                .srcFamilyIdx = srcFamily,
                                .dstFamilyIdx = dstFamily,
                                .id = buffer,
                                .offset = 0,
                                .size = vk::WholeSize};

    BufferBarrier const acquire{.srcStages = vk::PipelineStageFlagBits2::eNone,
                                .srcAccess = vk::AccessFlagBits2::eNone,
                                .dstStages = dst.stages,
                                .dstAccess = dst.access,
                                .srcFamilyIdx = srcFamily,
                                .dstFamilyIdx = dstFamily,
                                .id = buffer,
                                .offset = 0,
                                .size = vk::WholeSize};

    return {release, acquire};
}
} // namespace caldera::detail