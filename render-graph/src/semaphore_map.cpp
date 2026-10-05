#include <caldera/semaphore_map.h>

#include "detail/semaphore_map.h"
#include "detail/utility.h"

namespace caldera
{
    /* Impl */

    detail::Semaphore& SemaphoreMap::Impl::get_semaphore(FamilyType const type) noexcept
    {
        auto const index = static_cast<uint32_t>(detail::family_to_index(type));
        return semaphores[index];
    }

    /* Executor Attorney */

    detail::Semaphore& SemaphoreMap::ExecutorAttorney::get_semaphore(
        SemaphoreMap& map, FamilyType const type) noexcept
    {
        return map.m_impl->get_semaphore(type);
    }

    /* SemaphoreMap */

    SemaphoreMap::SemaphoreMap() :
        m_impl(std::make_unique<Impl>())
    {
        m_impl->semaphores.fill(detail::Semaphore{ VK_NULL_HANDLE, 0 });
    }

    SemaphoreMap::~SemaphoreMap() noexcept = default;

    SemaphoreMap::SemaphoreMap(SemaphoreMap &&) noexcept = default;
    SemaphoreMap & SemaphoreMap::operator=(SemaphoreMap &&) noexcept = default;

    void SemaphoreMap::map(
        FamilyType const family,
        vk::Semaphore const semaphore,
        uint64_t const timeline)
    {
        assert(semaphore != VK_NULL_HANDLE);

        auto& target = m_impl->get_semaphore(family);
        target.handle = semaphore;
        target.timeline = timeline;
    }

    void SemaphoreMap::unmap(FamilyType const family) noexcept {
        m_impl->get_semaphore(family).handle = VK_NULL_HANDLE;
    }

    void SemaphoreMap::update_timeline(FamilyType const family, uint64_t const timeline)
    {
        auto& target = m_impl->get_semaphore(family);
        assert(target.handle != VK_NULL_HANDLE);

        target.timeline = timeline;
    }

    uint64_t SemaphoreMap::get_timeline(FamilyType const family) const
    {
        auto& target = m_impl->get_semaphore(family);
        assert(target.handle != VK_NULL_HANDLE);

        return target.timeline;
    }
}
