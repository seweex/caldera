#pragma once

#include <array>
#include "structs.h"

namespace caldera::detail
{
    enum class HazardType {
        none,
        access,
        ownership
    };

    class AccessResolver
    {
    public:
        [[nodiscard]] HazardType has_hazards(
            ResourceTouch const& src,
            ResourceTouch const& dst) const noexcept;

        [[nodiscard]] ImageBarrier resolve_access_hazard(
            ImageID image, ResourceTouch const& src, ResourceTouch const& dst) const noexcept;

        [[nodiscard]] BufferBarrier resolve_access_hazard(
            BufferID buffer, ResourceTouch const& src, ResourceTouch const& dst) const noexcept;

        [[nodiscard]] std::array<ImageBarrier, 2> resolve_ownership_hazard(
            ImageID image, ResourceTouch const& src, ResourceTouch const& dst) const noexcept;

        [[nodiscard]] std::array<BufferBarrier, 2> resolve_ownership_hazard(
            BufferID buffer, ResourceTouch const& src, ResourceTouch const& dst) const noexcept;
    };
}
