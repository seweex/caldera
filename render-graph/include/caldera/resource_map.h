#pragma once

#include <memory>
#include <vulkan/vulkan.hpp>

#include "fwd.h"

namespace caldera
{
    class ResourceMap
    {
    public:
        ResourceMap();
        ~ResourceMap() noexcept;

        ResourceMap(ResourceMap &&) noexcept;
        ResourceMap& operator=(ResourceMap &&) noexcept;

        ResourceMap(const ResourceMap &) = delete;
        ResourceMap & operator=(const ResourceMap &) = delete;

        [[nodiscard]] ImageID alias(vk::Image resource = VK_NULL_HANDLE);
        [[nodiscard]] BufferID alias(vk::Buffer resource = VK_NULL_HANDLE);

        void map(ImageID id, vk::Image resource);
        void map(BufferID id, vk::Buffer resource);

        void unmap(ImageID id) noexcept;
        void unmap(BufferID id) noexcept;

        [[nodiscard]] vk::Image at(ImageID id) const;
        [[nodiscard]] vk::Buffer at(BufferID id) const;

        void unmap_all() noexcept;

        struct TransientAttorney;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
