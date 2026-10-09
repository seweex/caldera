#include <framework/allocator.h>
#include <vk_mem_alloc.h>

#include "detail/utility.h"

namespace
{
    [[nodiscard]] VmaAllocator create_allocator(
        vk::Instance const instance,
        vk::PhysicalDevice const physicalDevice,
        vk::Device const device,
        uint32_t const api)
    {
        VmaAllocatorCreateInfo createInfo {};
        createInfo.physicalDevice = physicalDevice;
        createInfo.device = device;
        createInfo.instance = instance;
        createInfo.vulkanApiVersion = api;

        VmaAllocator allocator;
        auto const rCode = vmaCreateAllocator(&createInfo, &allocator);

        caldera::framework::validate_result(rCode, "failed to create allocator");
        return allocator;
    }
}

namespace caldera::framework
{
    struct Allocator::Impl
    {
        vk::Device device;
        VmaAllocator allocator;
    };

    Allocator::Allocator(
        vk::Instance const instance,
        vk::PhysicalDevice const physicalDevice,
        vk::Device const device,
        uint32_t const api)
    :
        m_impl(std::make_unique<Impl>(
            device, ::create_allocator(instance, physicalDevice, device, api)))
    {}

    Allocator::~Allocator() noexcept = default;

    Image Allocator::make_image(vk::ImageCreateInfo const &info)
    {
        VkImageCreateInfo const imageInfo = info;

        VmaAllocationCreateInfo allocationInfo {};
        allocationInfo.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        VkImage image;
        VmaAllocation allocation;

        auto const rCode = vmaCreateImage(
            m_impl->allocator, &imageInfo, &allocationInfo, &image, &allocation, nullptr);

        validate_result(rCode, "failed to create image");
        return { image, allocation };
    }

    Buffer Allocator::make_buffer(vk::BufferCreateInfo const &info, bool const preferHostMemory)
    {
        VkBufferCreateInfo const bufferInfo = info;

        VmaAllocationCreateInfo allocationInfo {};
        allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;

        if (preferHostMemory)
            allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                   VMA_ALLOCATION_CREATE_MAPPED_BIT;
        else
            allocationInfo.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        VkBuffer buffer;
        VmaAllocation allocation;
        VmaAllocationInfo mapInfo;

        auto const rCode = vmaCreateBuffer(
            m_impl->allocator, &bufferInfo, &allocationInfo, &buffer, &allocation, &mapInfo);

        validate_result(rCode, "failed to create buffer");
        return { buffer, allocation, mapInfo.pMappedData };
    }

    void Allocator::destroy(Image const image) noexcept {
        vmaDestroyImage(m_impl->allocator, image.image, image.allocation);
    }

    void Allocator::destroy(Buffer const buffer) noexcept {
        vmaDestroyBuffer(m_impl->allocator, buffer.buffer, buffer.allocation);
    }

    VmaAllocator Allocator::get_allocator() const noexcept {
        return m_impl->allocator;
    }
}
