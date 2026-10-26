#pragma once

#include <vector>
#include <span>
#include <vulkan/vulkan.hpp>

#include <caldera/fwd.hpp>

namespace caldera
{
    /// @brief Class for mapping vulkan resource IDs to its handles
    class ResourceMap
    {
    public:
        /**
         * @brief Maps new image handle to a new image ID
         *
         * @param image Image handle for mapping or VK_NULL_HANDLE
         * @return ID of the newly mapped image
         */
        [[nodiscard]] ImageID map(vk::Image image);

        /**
         * @brief Maps new buffer handle to a new buffer ID
         *
         * @param buffer Buffer handle for mapping or VK_NULL_HANDLE
         * @return ID of the newly mapped buffer
         */
        [[nodiscard]] BufferID map(vk::Buffer buffer);

        /**
         * @brief Maps an image handle to an existing image ID
         *
         * @param id Existing image ID for mapping to
         * @param image Image handle for mapping or VK_NULL_HANDLE
         */
        void map(ImageID id, vk::Image image);

        /**
         * @brief Maps a buffer handle to an existing buffer ID
         *
         * @param id Existing buffer ID for mapping to
         * @param buffer Buffer handle for mapping or VK_NULL_HANDLE
         */
        void map(BufferID id, vk::Buffer buffer);

        /**
         * @brief Gets the image handle mapped to an image ID
         *
         * @param id Image ID to get handle mapped to
         * @return Image resource handle mapped to the specified ID or VK_NULL_HANDLE
         *
         * @note During an execution, you can access only to the resources
         * the pass reads or writes. Access to unallowed resource occurs to assert failure
         */
        [[nodiscard]] vk::Image at(ImageID id) const;

        /**
         * @brief Gets the buffer handle mapped to a buffer ID
         *
         * @param id Buffer ID to get handle mapped to
         * @return Buffer resource handle mapped to the specified ID or VK_NULL_HANDLE
         *
         * @note During an execution, you can access only to the resources
         * the pass reads or writes. Access to unallowed resource occurs to assert failure
         */
        [[nodiscard]] vk::Buffer at(BufferID id) const;

    private:
        void bind_resources(detail::GraphResources const& resources) noexcept;

        void bind_resource_filters(
            std::span<ImageID const> images,
            std::span<BufferID const> buffers) noexcept;

        friend class GraphExecutor;

        std::vector<vk::Image> m_persistentImages;
        std::vector<vk::Buffer> m_persistentBuffers;

        std::span<ImageID const> m_allowedImages;
        std::span<BufferID const> m_allowedBuffers;

        detail::GraphResources const *m_transientResources = nullptr;
    };
}
