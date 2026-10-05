#pragma once

#include <cstdint>
#include "fwd.h"

namespace caldera
{
    struct ImageTag {};
    struct BufferTag {};

    template <class TagTy>
    struct BasicResourceID
    {
        uint32_t is_transient : 1;
        uint32_t index : 31;

        [[nodiscard]] auto operator<=>(BasicResourceID const&) const noexcept = default;
    };

    template <class TagTy>
    struct BasicResourceVersion : BasicResourceID<TagTy>
    {
        uint32_t version;

        [[nodiscard]] auto operator<=>(BasicResourceVersion const&) const noexcept = default;
    };
}

namespace std
{
    template <class TagTy>
    struct hash<caldera::BasicResourceID<TagTy>>
    {
        [[nodiscard]] size_t operator()(caldera::BasicResourceID<TagTy> const id) const noexcept {
            return std::hash<uint32_t>()((id.is_transient << 31) | id.index);
        }
    };
}