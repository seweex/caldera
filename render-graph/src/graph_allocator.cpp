
#include "detail/graph_allocator.hpp"

namespace caldera {
detail::GraphAllocator::GraphAllocator(vk::Device const device, VmaAllocator const allocator,
                                       FamilyNumbers const &families) noexcept
    : m_device(device), m_allocator(allocator), m_families(families) {}

std::expected<detail::GraphResources, vk::Result>
detail::GraphAllocator::allocate(GraphIR const &ir, ImageTrackingTable const &imageTracking,
                                 BufferTrackingTable const &bufferTracking) {
    auto const imageCount = ir.transientImages.size();
    auto const bufferCount = ir.transientBuffers.size();
    auto const imageViewCount = ir.imageViewRequests.size();

    GraphResources result;
    result.transientImages.reserve(imageCount);
    result.transientBuffers.reserve(bufferCount);
    result.transientImageViews.reserve(imageViewCount);

    result.imageHeaps.reserve(imageCount);
    result.bufferHeaps.reserve(bufferCount);

    auto rCode = append_with_allocated<ImageTag>(m_device, m_allocator, m_families, ir.transientImages, imageTracking,
                                                 result.transientImages, result.imageHeaps);

    if (rCode < vk::Result::eSuccess) {
        deallocate(result);
        return std::unexpected(rCode);
    }

    rCode = append_with_allocated<BufferTag>(m_device, m_allocator, m_families, ir.transientBuffers, bufferTracking,
                                             result.transientBuffers, result.bufferHeaps);

    if (rCode < vk::Result::eSuccess) {
        deallocate(result);
        return std::unexpected(rCode);
    }

    rCode = append_with_created_image_views(m_device, ir.imageViewRequests, result.transientImages, result.transientImageViews);

    if (rCode < vk::Result::eSuccess) {
        deallocate(result);
        return std::unexpected(rCode);
    }

    return result;
}

void detail::GraphAllocator::deallocate(GraphResources &resources) {
    destroy_images(m_device, resources.transientImages);
    destroy_buffers(m_device, resources.transientBuffers);
    destroy_image_views(m_device, resources.transientImageViews);

    destroy_heaps(m_allocator, resources.imageHeaps);
    destroy_heaps(m_allocator, resources.bufferHeaps);

    resources = {};
}

bool detail::GraphAllocator::is_valid() const noexcept {
    assert((m_allocator != nullptr) == (m_device != nullptr));
    return m_allocator != nullptr && m_device != nullptr;
}
void detail::GraphAllocator::reset() noexcept {
    m_device = nullptr;
    m_allocator = nullptr;
}
} // namespace caldera
