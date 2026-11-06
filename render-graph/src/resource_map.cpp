#include <cassert>
#include <cstdint>
#include <ranges>

#include <caldera/resource_id.hpp>
#include <caldera/resource_map.hpp>
#include <caldera/structs.hpp>

namespace {
[[nodiscard]] auto get_resource(auto const id, auto const &persistentResources, auto const &transientResources,
                                auto const &filter) {
    assert(std::ranges::is_sorted(filter));

    if (!filter.empty())
        assert(std::ranges::binary_search(filter, id) && "unallowed resource was requested");

    if (id.isTransient)
        return transientResources[id.index].handle;
    else
        return persistentResources[id.index];
}

[[nodiscard]] auto get_view(auto const id, auto const &persistentViewList, auto const &transientViewList,
                            auto const &filter) {
    if (id.image.isTransient) {
        assert(std::ranges::is_sorted(filter));

        if (!filter.empty())
            assert(std::ranges::binary_search(filter, id) && "unallowed view was requested");

        return transientViewList[id.index].handle;
    } else {
        auto const [refImage, handle] = persistentViewList[id.index];
        assert(refImage == id.image && "the creation image and the requested one are not the same");

        return handle;
    }
}

auto map_resource(auto &list, auto const id, auto const handle) {
    assert(!id.isTransient && "mapping of a transient resource is not allowed");

    if (list.size() <= id.index)
        list.resize(id.index + 1);

    list[id.index] = handle;
    return id;
}

auto map_view(auto &list, auto const id, auto const handle) {
    assert(!id.image.isTransient && "manual mapping of a view for a transient resource is not allowed");

    if (list.size() <= id.index)
        list.resize(id.index + 1);

    list[id.index].first = id.image;
    list[id.index].second = handle;

    return id;
}

template <class TagTy> [[nodiscard]] auto generate_id(auto const &list) {
    return caldera::BasicResourceID<TagTy>{.isTransient = 0, .index = static_cast<uint32_t>(list.size())};
}

[[nodiscard]] caldera::ImageViewID generate_id(auto const &list, caldera::ImageID const image) {
    return caldera::ImageViewID{.image = image, .index = static_cast<uint32_t>(list.size())};
}
} // namespace

namespace caldera {
ImageID ResourceMap::map(vk::Image const image) {
    auto const id = ::generate_id<ImageTag>(m_persistentImages);
    return ::map_resource(m_persistentImages, id, image);
}

BufferID ResourceMap::map(vk::Buffer const buffer) {
    auto const id = ::generate_id<BufferTag>(m_persistentBuffers);
    return ::map_resource(m_persistentBuffers, id, buffer);
}

ImageViewID ResourceMap::map(ImageID const image, vk::ImageView const view) {
    auto const id = ::generate_id(m_persistentImageViews, image);
    return ::map_view(m_persistentImageViews, id, view);
}

void ResourceMap::map(ImageID const id, vk::Image const image) { ::map_resource(m_persistentImages, id, image); }

void ResourceMap::map(BufferID const id, vk::Buffer const buffer) { ::map_resource(m_persistentBuffers, id, buffer); }

void ResourceMap::map(ImageViewID const id, vk::ImageView const view) { ::map_view(m_persistentImageViews, id, view); }

vk::Image ResourceMap::at(ImageID const id) const {
    return ::get_resource(id, m_persistentImages, m_transientResources->transientImages, m_allowedImages);
}

vk::Buffer ResourceMap::at(BufferID const id) const {
    return ::get_resource(id, m_persistentBuffers, m_transientResources->transientBuffers, m_allowedBuffers);
}

vk::ImageView ResourceMap::at(ImageViewID const id) const {
    return ::get_view(id, m_persistentImageViews, m_transientResources->transientImageViews, m_allowedImageViews);
}

void ResourceMap::bind_resources(detail::GraphResources const &resources) noexcept {
    m_transientResources = &resources;
}

void ResourceMap::bind_resource_filters(std::span<ImageID const> const images, std::span<BufferID const> const buffers,
                                        std::span<ImageViewID const> const imageViews) noexcept {
    m_allowedImages = images;
    m_allowedBuffers = buffers;
    m_allowedImageViews = imageViews;
}
} // namespace caldera
