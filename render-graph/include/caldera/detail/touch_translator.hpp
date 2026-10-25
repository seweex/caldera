#pragma once

#include <cstdint>
#include <vulkan/vulkan.hpp>

#include <caldera/fwd.hpp>

namespace caldera::detail
{
    class TouchTranslator
    {
    public:
        [[nodiscard]] ImageTouch translate_touch(
            FamilyType type,
            ImageUsage usage,
            AccessKind access,
            uint32_t passIdx) const noexcept;

        [[nodiscard]] BufferTouch translate_touch(
            FamilyType type,
            BufferUsage usage,
            AccessKind access,
            uint32_t passIdx) const noexcept;

        [[nodiscard]] vk::ImageUsageFlags
        translate_to_usage_flags(ImageTouch const& touch) const noexcept;

        [[nodiscard]] vk::BufferUsageFlags
        translate_to_usage_flags(BufferTouch const& touch) const noexcept;

        [[nodiscard]] vk::ImageAspectFlags
        translate_to_aspect_mask(ImageTouch const& touch) const noexcept;

        [[nodiscard]] vk::ImageAspectFlags
        translate_to_aspect_mask(vk::Format format) const noexcept;
    };
}