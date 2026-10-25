#pragma once

#include <array>
#include <caldera/fwd.hpp>

namespace caldera::detail
{
    class AccessResolver
    {
    public:
        [[nodiscard]] HazardType has_hazards(
            FamilyNumbers const& families,
            ImageTouch const& src,
            ImageTouch const& dst) const noexcept;

        [[nodiscard]] HazardType has_hazards(
            FamilyNumbers const& families,
            BufferTouch const& src,
            BufferTouch const& dst) const noexcept;

        [[nodiscard]] ImageBarrier resolve_access_hazard(
            ImageID image,
            ImageTouch const& src,
            ImageTouch const& dst,
            ImageTrackedInfo const& trackedInfo) const noexcept;

        [[nodiscard]] BufferBarrier resolve_access_hazard(
            BufferID buffer,
            BufferTouch const& src,
            BufferTouch const& dst,
            [[maybe_unused]] BufferTrackedInfo const& trackedInfo) const noexcept;

        [[nodiscard]] std::array<ImageBarrier, 2> resolve_ownership_hazard(
            ImageID image,
            FamilyNumbers const& families,
            ImageTouch const& src,
            ImageTouch const& dst,
            ImageTrackedInfo const& trackedInfo) const noexcept;

        [[nodiscard]] std::array<BufferBarrier, 2> resolve_ownership_hazard(
            BufferID buffer,
            FamilyNumbers const& families,
            BufferTouch const& src,
            BufferTouch const& dst,
            [[maybe_unused]] BufferTrackedInfo const& trackedInfo) const noexcept;
    };
}
