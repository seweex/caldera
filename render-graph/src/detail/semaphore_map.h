#pragma once

#include <array>
#include <cstdint>

#include <caldera/semaphore_map.h>
#include <caldera/structs.h>

namespace caldera::detail
{
    struct Semaphore
    {
        vk::Semaphore handle;
        uint64_t timeline;
    };
}

namespace caldera
{
    struct SemaphoreMap::Impl
    {
        [[nodiscard]] detail::Semaphore& get_semaphore(FamilyType type) noexcept;

        std::array<detail::Semaphore, 3> semaphores;
    };

    struct SemaphoreMap::ExecutorAttorney
    {
        [[nodiscard]] static detail::Semaphore&
        get_semaphore(SemaphoreMap& map, FamilyType type) noexcept;
    };
}