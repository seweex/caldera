#include <cstdint>
#include <cassert>
#include <ranges>

#include <caldera/resource_id.hpp>
#include <caldera/structs.hpp>
#include <caldera/resource_map.hpp>

namespace
{
    [[nodiscard]] auto get(
        auto const id,
        auto const& persistentResources,
        auto const& transientResources,
        auto const& filter)
    {
        assert(std::ranges::is_sorted(filter));

        if (!filter.empty())
            assert(std::ranges::binary_search(filter, id) && "unallowed resource was requested");

        if (id.isTransient)
            return transientResources[id.index].handle;
        else
            return persistentResources[id.index];
    }

    auto map(
        auto& list,
        auto const id,
        auto const handle)
    {
        assert(!id.isTransient && "mapping of a transient resource is not allowed");

        if (list.size() <= id.index)
            list.resize(id.index + 1);

        list[id.index] = handle;
        return id;
    }

    template <class TagTy>
    [[nodiscard]] auto generate_id(auto const& list)
    {
        return caldera::BasicResourceID<TagTy> {
            .isTransient = 0,
            .index = static_cast<uint32_t>(list.size())
        };
    }
}

namespace caldera
{
    ImageID ResourceMap::map(vk::Image const image) {
        auto const id = ::generate_id<ImageTag>(m_persistentImages);
        return ::map(m_persistentImages, id, image);
    }

    BufferID ResourceMap::map(vk::Buffer const buffer) {
        auto const id = ::generate_id<BufferTag>(m_persistentBuffers);
        return ::map(m_persistentBuffers, id, buffer);
    }

    void ResourceMap::map(ImageID const id, vk::Image const image) {
        ::map(m_persistentImages, id, image);
    }

    void ResourceMap::map(BufferID const id, vk::Buffer const buffer) {
        ::map(m_persistentBuffers, id, buffer);
    }

    vk::Image ResourceMap::at(ImageID const id) const {
        return ::get(id, m_persistentImages, m_transientResources->transientImages, m_allowedImages);
    }

    vk::Buffer ResourceMap::at(BufferID const id) const {
        return ::get(id, m_persistentBuffers, m_transientResources->transientBuffers, m_allowedBuffers);
    }

    void ResourceMap::bind_resources(detail::GraphResources const &resources) noexcept {
        m_transientResources = &resources;
    }

    void ResourceMap::bind_resource_filters(
        std::span<ImageID> const images,
        std::span<BufferID> const buffers) noexcept
    {
        m_allowedImages = images;
        m_allowedBuffers = buffers;
    }
}
