#include <caldera/detail/touch_translator.hpp>
#include <vulkan/vulkan_format_traits.hpp>

namespace
{
    [[nodiscard]] vk::PipelineStageFlags2
    get_shader_stages_by_family(caldera::FamilyType const family) noexcept
    {
        switch (family)
        {
        case caldera::FamilyType::graphics:
            return vk::PipelineStageFlagBits2::eVertexShader |
                   vk::PipelineStageFlagBits2::eGeometryShader |
                   vk::PipelineStageFlagBits2::eFragmentShader;
        case caldera::FamilyType::compute:
            return vk::PipelineStageFlagBits2::eComputeShader;
        default:
            assert(false);
        }
    }

    /* * * * */

    [[nodiscard]] vk::PipelineStageFlags2
    image_usage_stage(
        caldera::FamilyType const family,
        caldera::ImageUsage const usage) noexcept
    {
        using namespace caldera;
        using Stage = vk::PipelineStageFlagBits2;

        switch (usage)
        {
        case ImageUsage::transfer:                 return Stage::eAllTransfer;
        case ImageUsage::sampled:                  [[fallthrough]];
        case ImageUsage::storage:                  return ::get_shader_stages_by_family(family);
        case ImageUsage::color_attachment:         return Stage::eColorAttachmentOutput;
        case ImageUsage::depth_stencil_attachment: return Stage::eEarlyFragmentTests | Stage::eLateFragmentTests;
        case ImageUsage::input_attachment:         return Stage::eFragmentShader;
        case ImageUsage::resolve_attachment:       return Stage::eColorAttachmentOutput;
        case ImageUsage::host:                     return Stage::eHost;
        default: assert(false);
        }
    }

    [[nodiscard]] vk::ImageLayout
    image_usage_layout(
        caldera::ImageUsage const usage,
        caldera::detail::AccessKind const access) noexcept
    {
        using caldera::ImageUsage;
        using caldera::detail::AccessKind;
        using Layout = vk::ImageLayout;

        switch (usage)
        {
        case ImageUsage::transfer:
            assert(access != AccessKind::read_write);
            return access == AccessKind::write ? Layout::eTransferDstOptimal : Layout::eTransferSrcOptimal;

        case ImageUsage::sampled:
            assert(access == AccessKind::read);
            return Layout::eShaderReadOnlyOptimal;

        case ImageUsage::storage:
            return Layout::eGeneral;

        case ImageUsage::color_attachment:
            return Layout::eColorAttachmentOptimal;

        case ImageUsage::depth_stencil_attachment:
            return access == AccessKind::read
                ? Layout::eDepthStencilReadOnlyOptimal
                : Layout::eDepthStencilAttachmentOptimal;

        case ImageUsage::input_attachment:
            assert(access == AccessKind::read);
            return Layout::eShaderReadOnlyOptimal;

        case ImageUsage::resolve_attachment:
            assert(access == AccessKind::write);
            return Layout::eColorAttachmentOptimal;

        case ImageUsage::host:
            return Layout::eGeneral;

        default:
            assert(false);
        }
    }

    [[nodiscard]] vk::AccessFlags2
    image_usage_access(
        caldera::ImageUsage const usage,
        caldera::detail::AccessKind const access) noexcept
    {
        using caldera::ImageUsage;
        using caldera::detail::AccessKind;
        using Access = vk::AccessFlagBits2;

        switch (usage)
        {
        case ImageUsage::transfer:
            return access == AccessKind::read ? Access::eTransferRead : Access::eTransferWrite;

        case ImageUsage::sampled:
            assert(access == AccessKind::read);
            return Access::eShaderSampledRead;

        case ImageUsage::storage:
            switch (access) {
                case AccessKind::read:       return Access::eShaderStorageRead;
                case AccessKind::write:      return Access::eShaderStorageWrite;
                case AccessKind::read_write: return Access::eShaderStorageRead | Access::eShaderStorageWrite;
            }
            break;

        case ImageUsage::color_attachment:
            switch (access) {
                case AccessKind::read:       return Access::eColorAttachmentRead;
                case AccessKind::write:      return Access::eColorAttachmentWrite;
                case AccessKind::read_write: return Access::eColorAttachmentRead | Access::eColorAttachmentWrite;
            }
            break;

        case ImageUsage::depth_stencil_attachment:
            switch (access) {
                case AccessKind::read:       return Access::eDepthStencilAttachmentRead;
                case AccessKind::write:      return Access::eDepthStencilAttachmentWrite;
                case AccessKind::read_write: return Access::eDepthStencilAttachmentRead |
                                                    Access::eDepthStencilAttachmentWrite;
            }
            break;

        case ImageUsage::input_attachment:
            assert(access == AccessKind::read);
            return Access::eInputAttachmentRead;

        case ImageUsage::resolve_attachment:
            assert(access == AccessKind::write);
            return Access::eColorAttachmentWrite;

        case ImageUsage::host:
            switch (access) {
                case AccessKind::read:      return Access::eHostRead;
                case AccessKind::write:     return Access::eHostWrite;
                case AccessKind::read_write: return Access::eHostRead | Access::eHostWrite;
            }
            break;
        }

        assert(false);
    }

    /* * * * */

    [[nodiscard]] vk::PipelineStageFlags2
    buffer_usage_stage(
        caldera::FamilyType const family,
        caldera::BufferUsage const usage) noexcept
    {
        using caldera::BufferUsage;
        using Stage = vk::PipelineStageFlagBits2;

        switch (usage)
        {
        case BufferUsage::transfer: return Stage::eAllTransfer;
        case BufferUsage::vertices: return Stage::eVertexInput;
        case BufferUsage::indices:  return Stage::eIndexInput;
        case BufferUsage::uniform:  return ::get_shader_stages_by_family(family);
        case BufferUsage::storage:  return ::get_shader_stages_by_family(family);
        case BufferUsage::host:     return Stage::eHost;
        }

        assert(false);
    }

    [[nodiscard]] vk::AccessFlags2
    buffer_usage_access(
        caldera::BufferUsage const usage,
        caldera::detail::AccessKind const access) noexcept
    {
        using caldera::BufferUsage;
        using caldera::detail::AccessKind;
        using Access = vk::AccessFlagBits2;

        switch (usage)
        {
        case BufferUsage::transfer:
            return access == AccessKind::read ? Access::eTransferRead : Access::eTransferWrite;

        case BufferUsage::vertices:
            assert(access == AccessKind::read);
            return Access::eVertexAttributeRead;

        case BufferUsage::indices:
            assert(access == AccessKind::read);
            return Access::eIndexRead;

        case BufferUsage::uniform:
            assert(access == AccessKind::read);
            return Access::eUniformRead;

        case BufferUsage::storage:
            switch (access) {
                case AccessKind::read:       return Access::eShaderStorageRead;
                case AccessKind::write:      return Access::eShaderStorageWrite;
                case AccessKind::read_write: return Access::eShaderStorageRead | Access::eShaderStorageWrite;
            }
            break;

        case BufferUsage::host:
            switch (access) {
                case AccessKind::read:       return Access::eHostRead;
                case AccessKind::write:      return Access::eHostWrite;
                case AccessKind::read_write: return Access::eHostRead | Access::eHostWrite;
            }
            break;
        }

        assert(false);
    }
}

namespace caldera::detail
{
    ImageTouch TouchTranslator::translate_touch(
        FamilyType const type,
        ImageUsage const usage,
        AccessKind const access,
        uint32_t const passIdx) const noexcept
    {
        return ImageTouch {
            .stages = ::image_usage_stage(type, usage),
            .access = ::image_usage_access(usage, access),
            .layout = ::image_usage_layout(usage, access),
            .family = type,
            .passIdx = passIdx
        };
    }

    BufferTouch TouchTranslator::translate_touch(
        FamilyType const type,
        BufferUsage const usage,
        AccessKind const access,
        uint32_t const passIdx) const noexcept
    {
        return BufferTouch {
            .stages = ::buffer_usage_stage(type, usage),
            .access = ::buffer_usage_access(usage, access),
            .family = type,
            .passIdx = passIdx
        };
    }

    vk::ImageUsageFlags TouchTranslator::translate_to_usage_flags(
        ImageTouch const& touch) const noexcept
    {
        using Access = vk::AccessFlagBits2;
        using Usage = vk::ImageUsageFlagBits;

        vk::ImageUsageFlags usage = {};

        if (touch.access & Access::eTransferRead)
            usage |= Usage::eTransferSrc;

        if (touch.access & Access::eTransferWrite)
            usage |= Usage::eTransferDst;

        if (touch.access & Access::eShaderSampledRead)
            usage |= Usage::eSampled;

        if (touch.access & (Access::eShaderStorageRead | Access::eShaderStorageWrite))
            usage |= Usage::eStorage;

        if (touch.access & (Access::eColorAttachmentRead | Access::eColorAttachmentWrite))
            usage |= Usage::eColorAttachment;

        if (touch.access & (Access::eDepthStencilAttachmentRead | Access::eDepthStencilAttachmentWrite))
            usage |= Usage::eDepthStencilAttachment;

        if (touch.access & Access::eInputAttachmentRead)
            usage |= Usage::eInputAttachment;

        return usage;
    }
    
    vk::BufferUsageFlags TouchTranslator::translate_to_usage_flags(
        BufferTouch const& touch) const noexcept
    {
        using Access = vk::AccessFlagBits2;
        using Usage = vk::BufferUsageFlagBits;

        vk::BufferUsageFlags usage = {};

        if (touch.access & Access::eTransferRead)
            usage |= Usage::eTransferSrc;

        if (touch.access & Access::eTransferWrite)
            usage |= Usage::eTransferDst;

        if (touch.access & Access::eVertexAttributeRead)
            usage |= Usage::eVertexBuffer;

        if (touch.access & Access::eIndexRead)
            usage |= Usage::eIndexBuffer;

        if (touch.access & Access::eUniformRead)
            usage |= Usage::eUniformBuffer;

        if (touch.access & (Access::eShaderStorageRead | Access::eShaderStorageWrite))
            usage |= Usage::eStorageBuffer;

        return usage;
    }

    vk::ImageAspectFlags TouchTranslator::translate_to_aspect_mask(
        ImageTouch const& touch) const noexcept
    {
        using Access = vk::AccessFlagBits2;
        using Aspect = vk::ImageAspectFlagBits;

        if (touch.access & (Access::eDepthStencilAttachmentRead |
                            Access::eDepthStencilAttachmentWrite))
        {
            return Aspect::eDepth | Aspect::eStencil;
        }

        return Aspect::eColor;
    }

    vk::ImageAspectFlags TouchTranslator::translate_to_aspect_mask(
        vk::Format const format) const noexcept {
        using Aspect = vk::ImageAspectFlagBits;

        vk::ImageAspectFlags result = {};

        if (vk::hasRedComponent(format) ||
            vk::hasGreenComponent(format) ||
            vk::hasBlueComponent(format) ||
            vk::hasAlphaComponent(format))
        {
            result |= Aspect::eColor;
        }

        if (vk::hasDepthComponent(format))
            result |= Aspect::eDepth;

        if (vk::hasStencilComponent(format))
            result |= Aspect::eStencil;

        return result;
    }
}
