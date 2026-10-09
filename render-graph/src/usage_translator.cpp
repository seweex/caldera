#include "detail/usage_translator.h"

namespace
{
    constexpr auto any_shader_stage =
        vk::PipelineStageFlagBits2::eVertexShader |
        vk::PipelineStageFlagBits2::eGeometryShader |
        vk::PipelineStageFlagBits2::eFragmentShader |
        vk::PipelineStageFlagBits2::eComputeShader;

    [[nodiscard]] vk::PipelineStageFlags2
    image_usage_stage(caldera::ImageUsage const usage) noexcept
    {
        using namespace caldera;
        using Stage = vk::PipelineStageFlagBits2;

        switch (usage)
        {
        case ImageUsage::transfer:                 return Stage::eAllTransfer;
        case ImageUsage::sampled:                  [[fallthrough]];
        case ImageUsage::storage:                  return any_shader_stage;
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

    [[nodiscard]] vk::ImageUsageFlags
    image_usage_to_vk(
        caldera::ImageUsage const usage,
        caldera::detail::AccessKind const access) noexcept
    {
        using caldera::ImageUsage;
        using caldera::detail::AccessKind;
        using Usage = vk::ImageUsageFlagBits;

        switch (usage)
        {
            case ImageUsage::transfer:
                switch (access) {
                    case AccessKind::read:       return Usage::eTransferSrc;
                    case AccessKind::write:      return Usage::eTransferDst;
                    case AccessKind::read_write: return Usage::eTransferSrc | Usage::eTransferDst;
                }
                break;

            case ImageUsage::sampled:
                assert(access == AccessKind::read);
                return Usage::eSampled;

            case ImageUsage::storage:
                return Usage::eStorage;

            case ImageUsage::color_attachment:
                return Usage::eColorAttachment;

            case ImageUsage::depth_stencil_attachment:
                return Usage::eDepthStencilAttachment;

            case ImageUsage::input_attachment:
                assert(access == AccessKind::read);
                return Usage::eInputAttachment;

            case ImageUsage::resolve_attachment:
                assert(access == AccessKind::write);
                return Usage::eColorAttachment;

            case ImageUsage::host:
                return {};
        }

        assert(false);
    }

    [[nodiscard]] vk::PipelineStageFlags2
    buffer_usage_stage(caldera::BufferUsage const usage) noexcept
    {
        using caldera::BufferUsage;
        using Stage = vk::PipelineStageFlagBits2;

        switch (usage)
        {
        case BufferUsage::transfer: return Stage::eAllTransfer;
        case BufferUsage::vertices: return Stage::eVertexInput;
        case BufferUsage::indices:  return Stage::eIndexInput;
        case BufferUsage::uniform:  return any_shader_stage;
        case BufferUsage::storage:  return any_shader_stage;
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

    [[nodiscard]] vk::BufferUsageFlags
    buffer_usage_to_vk(
        caldera::BufferUsage const usage,
        caldera::detail::AccessKind const access) noexcept
    {
        using caldera::BufferUsage;
        using caldera::detail::AccessKind;
        using Usages = vk::BufferUsageFlagBits;

        switch (usage)
        {
            case BufferUsage::transfer:
                switch (access) {
                    case AccessKind::read:       return Usages::eTransferSrc;
                    case AccessKind::write:      return Usages::eTransferDst;
                    case AccessKind::read_write: return Usages::eTransferSrc | Usages::eTransferDst;
                }
                break;

            case BufferUsage::vertices:
                assert(access == AccessKind::read);
                return Usages::eVertexBuffer;

            case BufferUsage::indices:
                assert(access == AccessKind::read);
                return Usages::eIndexBuffer;

            case BufferUsage::uniform:
                assert(access == AccessKind::read);
                return Usages::eUniformBuffer;

            case BufferUsage::storage:
                return Usages::eStorageBuffer;

            case BufferUsage::host:
                return {};
        }

        assert(false);
    }
}

namespace caldera::detail
{
    ResourceTouch UsageTranslator::translate_usage(
        FamilyType const family,
        ImageUsage const usage,
        AccessKind const access) const noexcept
    {
        return ResourceTouch {
            .stages = ::image_usage_stage(usage),
            .access = ::image_usage_access(usage, access),
            .layout = ::image_usage_layout(usage, access),
            .family = family
        };
    }

    ResourceTouch UsageTranslator::translate_usage(
        FamilyType const family,
        BufferUsage const usage,
        AccessKind const access) const noexcept
    {
        return ResourceTouch {
            .stages = ::buffer_usage_stage(usage),
            .access = ::buffer_usage_access(usage, access),
            .layout = vk::ImageLayout::eUndefined,
            .family = family
        };
    }

    vk::ImageUsageFlags UsageTranslator::translate_flags(
        ImageUsage const usage,
        AccessKind const access) const noexcept
    {
        return ::image_usage_to_vk(usage, access);
    }

    vk::BufferUsageFlags UsageTranslator::translate_flags(
        BufferUsage const usage,
        AccessKind const access) const noexcept
    {
        return ::buffer_usage_to_vk(usage, access);
    }

    vk::ImageAspectFlags UsageTranslator::get_aspect(
        ImageUsage const usage) const noexcept
    {
        switch (usage)
        {
        case ImageUsage::transfer:                  return {};
        case ImageUsage::sampled:                   return {};
        case ImageUsage::storage:                   return {};
        case ImageUsage::color_attachment:          return vk::ImageAspectFlagBits::eColor;
        case ImageUsage::depth_stencil_attachment:  return vk::ImageAspectFlagBits::eDepth;
        case ImageUsage::input_attachment:          return {};
        case ImageUsage::resolve_attachment:        return {};
        case ImageUsage::host:                      return {};
        }
    }
}
