#pragma once

#include <cstdint>
#include <functional>
#include "fwd.h"

namespace caldera
{
    struct ImageTag {};
    struct BufferTag {};

    template <class TagTy>
    struct BasicResourceID
    {
        static constexpr int transient_flag_bits = 1;
        static constexpr int index_bits = 13;
        static constexpr int version_bits = 18;

        uint32_t isTransient : transient_flag_bits;
        uint32_t index       : index_bits;
        uint32_t version     : version_bits;

        [[nodiscard]] constexpr auto operator<=>(BasicResourceID const&) const noexcept = default;
    };

    static_assert(sizeof(uint32_t) == sizeof(BasicResourceID<ImageTag>));
    static_assert(sizeof(uint32_t) == sizeof(BasicResourceID<BufferTag>));
}

template <class TagTy>
struct std::hash<caldera::BasicResourceID<TagTy>>
{
    using ID = caldera::BasicResourceID<TagTy>;

    [[nodiscard]] size_t operator()(ID const id) const noexcept
    {
        auto const value =
            (static_cast<uint32_t>(id.isTransient) << (32 - ID::transient_flag_bits)) |
            (static_cast<uint32_t>(id.index) << (32 - ID::transient_flag_bits - ID::index_bits));

        return std::hash<uint32_t>()(value);
    }
};