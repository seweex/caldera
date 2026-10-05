#pragma once

#include <memory>
#include <vulkan/vulkan.hpp>

#include "fwd.h"

namespace caldera
{
    class SemaphoreMap
    {
    public:
        SemaphoreMap();
        ~SemaphoreMap() noexcept;

        SemaphoreMap(SemaphoreMap&&) noexcept;
        SemaphoreMap& operator=(SemaphoreMap&&) noexcept;

        SemaphoreMap(SemaphoreMap const&) = delete;
        SemaphoreMap& operator=(SemaphoreMap const&) = delete;

        void map(FamilyType family, vk::Semaphore semaphore, uint64_t timeline);
        void unmap(FamilyType family) noexcept;

        void update_timeline(FamilyType family, uint64_t timeline);
        [[nodiscard]] uint64_t get_timeline(FamilyType family) const;

        struct ExecutorAttorney;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
