#pragma once

#include "structs.h"

namespace caldera::detail
{
    class UsageTranslator
    {
    public:
        [[nodiscard]] ResourceTouch translate_usage(
            FamilyType family, ImageUsage usage, AccessKind access) const noexcept;

        [[nodiscard]] ResourceTouch translate_usage(
            FamilyType family, BufferUsage usage, AccessKind access) const noexcept;

        [[nodiscard]] vk::ImageUsageFlags translate_flags(
            ImageUsage usage, AccessKind access) const noexcept;

        [[nodiscard]] vk::BufferUsageFlags translate_flags(
            BufferUsage usage, AccessKind access) const noexcept;

        [[nodiscard]] vk::ImageAspectFlags get_aspect(ImageUsage usage) const noexcept;
    };
}