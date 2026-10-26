#pragma once

#include <cstddef>
#include <cstdint>
#include <compare>
#include <functional>

namespace caldera
{
    /// @brief Tag for image-related thing
    struct ImageTag {};

    /// @brief Tag for buffer-related things
    struct BufferTag {};

    /**
     * @brief Lightweight struct for identifying vulkan resources
     * @tparam TagTy Tag of the resource type ID is related on
     */
    template <class TagTy>
    struct BasicResourceID
    {
        static constexpr int transient_flag_bits = 1;
        static constexpr int index_bits = 31;

        uint32_t isTransient : transient_flag_bits;
        uint32_t index       : index_bits;

        [[nodiscard]] constexpr auto operator<=>(BasicResourceID const&) const noexcept = default;
    };

    static_assert(sizeof(uint32_t) == sizeof(BasicResourceID<ImageTag>));
    static_assert(sizeof(uint32_t) == sizeof(BasicResourceID<BufferTag>));

    /**
     * @brief Lightweight struct for identifying vulkan resource version
     * @tparam TagTy Tag of the resource type Version is related on
     */
    template <class TagTy>
    struct BasicResourceVersion
    {
        constexpr BasicResourceVersion() noexcept = default;
        constexpr BasicResourceVersion(BasicResourceID<TagTy> const resourceId) noexcept :
            id(resourceId),
            versionIdx(0)
        {}

        BasicResourceID<TagTy> id;
        uint32_t versionIdx;

        [[nodiscard]] constexpr auto operator<=>(BasicResourceVersion const&) const noexcept = default;
    };

    static_assert(sizeof(uint64_t) == sizeof(BasicResourceVersion<ImageTag>));
    static_assert(sizeof(uint64_t) == sizeof(BasicResourceVersion<BufferTag>));
}

namespace std
{
    template <class TagTy>
    struct hash<caldera::BasicResourceID<TagTy>>
    {
        using ID = caldera::BasicResourceID<TagTy>;

        [[nodiscard]] size_t operator()(ID const id) const noexcept
        {
            auto const value =
                (static_cast<uint32_t>(id.isTransient) << (32 - ID::transient_flag_bits)) |
                (static_cast<uint32_t>(id.index));

            return hash<uint32_t>()(value);
        }
    };

    template <class TagTy>
    struct hash<caldera::BasicResourceVersion<TagTy>>
    {
        using ID = caldera::BasicResourceID<TagTy>;
        using Version = caldera::BasicResourceVersion<TagTy>;

        [[nodiscard]] size_t operator()(Version const version) const noexcept
        {
            auto const idHash = hash<ID>()(version.id);

            return idHash ^ (std::hash<uint32_t>()(version.versionIdx) + 0x9e3779b9 + (idHash << 6) + (idHash >> 2));
        }
    };
}