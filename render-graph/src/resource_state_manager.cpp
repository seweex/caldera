#include <caldera/resource_state_manager.h>
#include <caldera/resource_id.h>

#include "detail/resource_state_manager.h"

namespace
{
    void reset_state(auto& list, auto const id) {
        assert(!id.is_transient);
        list[id.index] = caldera::detail::ResourceTouch{};
    }
}

namespace caldera
{
    /* Impl */

    void ResourceStateManager::Impl::cleanup_transient() noexcept {
        std::ranges::fill(transientImageTouches, detail::ResourceTouch{});
        std::ranges::fill(transientBufferTouches, detail::ResourceTouch{});
    }

    /* Executor Attorney */

    void ResourceStateManager::ExecutorAttorney::cleanup_transient(
        ResourceStateManager &manager) noexcept
    {
        manager.m_impl->cleanup_transient();
    }

    detail::ResourceTouch& ResourceStateManager::ExecutorAttorney::get_state(
        ResourceStateManager& manager,
        ImageID const id) noexcept
    {
        auto& touchList = (id.is_transient ? manager.m_impl->transientImageTouches : manager.m_impl->externImageTouches);

        if (touchList.size() <= id.index)
            touchList.resize(id.index + 1);

        return touchList[id.index];
    }

    detail::ResourceTouch& ResourceStateManager::ExecutorAttorney::get_state(
        ResourceStateManager& manager,
        BufferID const id) noexcept
    {
        auto& touchList = (id.is_transient ? manager.m_impl->transientImageTouches : manager.m_impl->externImageTouches);

        if (touchList.size() <= id.index)
            touchList.resize(id.index + 1);

        return touchList[id.index];
    }

    detail::ResourceTouch const& ResourceStateManager::ExecutorAttorney::get_state(
        ResourceStateManager const &manager,
        ImageID const id) noexcept
    {
        return get_state(const_cast<ResourceStateManager&>(manager), id);
    }

    detail::ResourceTouch const & ResourceStateManager::ExecutorAttorney::get_state(
        ResourceStateManager const &manager,
        BufferID const id) noexcept
    {
        return get_state(const_cast<ResourceStateManager&>(manager), id);
    }

    /* ResourceStateManager */

    ResourceStateManager::ResourceStateManager() :
        m_impl(std::make_unique<Impl>())
    {}

    ResourceStateManager::~ResourceStateManager() noexcept = default;

    ResourceStateManager::ResourceStateManager(ResourceStateManager &&) noexcept = default;
    ResourceStateManager & ResourceStateManager::operator=(ResourceStateManager &&) noexcept = default;

    void ResourceStateManager::reset_for(ImageID const id) noexcept {
        ::reset_state(m_impl->externImageTouches, id);
    }

    void ResourceStateManager::reset_for(BufferID const id) noexcept {
        ::reset_state(m_impl->externBufferTouches, id);
    }
}
