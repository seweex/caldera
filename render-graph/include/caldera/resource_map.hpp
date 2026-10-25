#pragma once

#include <vector>
#include <span>
#include <vulkan/vulkan.hpp>

#include <caldera/structs.hpp>

namespace caldera
{
    class ResourceMap
    {
    public:
        [[nodiscard]] ImageID map(vk::Image image);
        [[nodiscard]] BufferID map(vk::Buffer buffer);

        void map(ImageID id, vk::Image image);
        void map(BufferID id, vk::Buffer buffer);

        [[nodiscard]] vk::Image at(ImageID id) const;
        [[nodiscard]] vk::Buffer at(BufferID id) const;

    private:
        void bind_resources(detail::GraphResources const& resources) noexcept;
        void bind_resource_filters(std::span<ImageID> images, std::span<BufferID> buffers) noexcept;

        friend class GraphExecutor;

        std::vector<vk::Image> m_persistentImages;
        std::vector<vk::Buffer> m_persistentBuffers;

        std::span<ImageID> m_allowedImages;
        std::span<BufferID> m_allowedBuffers;

        detail::GraphResources const *m_transientResources = nullptr;
    };
}
