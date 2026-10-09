#pragma once

#include <memory>
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>

namespace caldera::framework
{
    struct Image {
        vk::Image image;
        VmaAllocation allocation;
    };

    struct Buffer {
        vk::Buffer buffer;
        VmaAllocation allocation;

        void* mapped;
    };

    class Allocator
    {
    public:
        Allocator(
            vk::Instance instance,
            vk::PhysicalDevice physicalDevice,
            vk::Device device,
            uint32_t api);

        ~Allocator() noexcept;

        Allocator(Allocator const&) = delete;
        Allocator& operator=(Allocator const&) = delete;

        [[nodiscard]] Image make_image(vk::ImageCreateInfo const& info);
        [[nodiscard]] Buffer make_buffer(vk::BufferCreateInfo const& info, bool preferHostMemory = false);

        void destroy(Image image) noexcept;
        void destroy(Buffer buffer) noexcept;

        [[nodiscard]] VmaAllocator get_allocator() const noexcept;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
