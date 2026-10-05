#include <caldera/resource_map.h>
#include <caldera/resource_id.h>

#include "detail/resource_map.h"

namespace
{
    [[nodiscard]] auto& choose_list(
        auto& transientResources,
        auto& externResources,
        auto const id) noexcept
    {
        return id.is_transient ? transientResources : externResources;
    }

    template <class TagTy>
    [[nodiscard]] auto alias_index(auto& externList, auto const resource)
    {
        auto const index = static_cast<uint32_t>(externList.size());

        externList.emplace_back(resource);
        return caldera::BasicResourceID<TagTy>{ 0, index };
    }

    void map_resource(
        auto& list,
        auto const id,
        auto const resource)
    {
        if (list.size() <= id.index)
            list.resize(id.index + 1);

        list[id.index] = resource;
    }

    [[nodiscard]] auto get_resource(auto& list, auto const id) {
        return list[id.index];
    }

    [[nodiscard]] auto unmap_list(auto& list) {
        for (auto& el : list)
            el = VK_NULL_HANDLE;
    }
}

namespace caldera
{
    /* Transient Attorney */

    void ResourceMap::TransientAttorney::import_graph(
        ResourceMap &map,
        detail::AllocatedGraph const &graph)
    {
        auto& impl = *map.m_impl;

        impl.transientImages = graph.images;
        impl.transientBuffers = graph.buffers;
    }

    void ResourceMap::TransientAttorney::reset_transient(ResourceMap &map) noexcept
    {
        auto& impl = *map.m_impl;

        ::unmap_list(impl.transientImages);
        ::unmap_list(impl.transientBuffers);
    }

    /* Resource Map */

    ResourceMap::ResourceMap() :
        m_impl(std::make_unique<Impl>())
    {}

    ResourceMap::~ResourceMap() noexcept = default;

    ResourceMap::ResourceMap(ResourceMap &&) noexcept = default;
    ResourceMap & ResourceMap::operator=(ResourceMap &&) noexcept = default;

    ImageID ResourceMap::alias(vk::Image const resource) {
        return ::alias_index<ImageTag>(m_impl->externImages, resource);
    }

    BufferID ResourceMap::alias(vk::Buffer const resource) {
        return ::alias_index<BufferTag>(m_impl->externBuffers, resource);
    }

    void ResourceMap::map(ImageID const id, vk::Image const resource) {
        auto& list = ::choose_list(m_impl->transientImages, m_impl->externImages, id);
        ::map_resource(list, id, resource);
    }

    void ResourceMap::map(BufferID const id, vk::Buffer const resource) {
        auto& list = ::choose_list(m_impl->transientBuffers, m_impl->externBuffers, id);
        ::map_resource(list, id, resource);
    }

    void ResourceMap::unmap(ImageID const id) noexcept {
        auto& list = ::choose_list(m_impl->transientImages, m_impl->externImages, id);
        ::map_resource(list, id, VK_NULL_HANDLE);
    }

    void ResourceMap::unmap(BufferID const id) noexcept {
        auto& list = ::choose_list(m_impl->transientBuffers, m_impl->externBuffers, id);
        ::map_resource(list, id, VK_NULL_HANDLE);
    }

    vk::Image ResourceMap::at(ImageID const id) const {
        auto& list = ::choose_list(m_impl->transientImages, m_impl->externImages, id);
        return ::get_resource(list, id);
    }

    vk::Buffer ResourceMap::at(BufferID const id) const {
        auto& list = ::choose_list(m_impl->transientBuffers, m_impl->externBuffers, id);
        return ::get_resource(list, id);
    }

    void ResourceMap::unmap_all() noexcept {
        ::unmap_list(m_impl->transientImages);
        ::unmap_list(m_impl->transientBuffers);
        ::unmap_list(m_impl->externImages);
        ::unmap_list(m_impl->externBuffers);
    }


}
