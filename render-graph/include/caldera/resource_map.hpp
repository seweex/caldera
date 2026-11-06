#pragma once

#include <span>
#include <vector>
#include <vulkan/vulkan.hpp>

#include <caldera/fwd.hpp>

namespace caldera {
/// @brief Class for mapping vulkan resource IDs to its handles
class ResourceMap {
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
     * @brief Maps new image view handle to a new image view ID
     *
     * @param image Image ID for which the image view was created
     * @param view Image view handle for mapping or VK_NULL_HANDLE
     * @return ID of the newly mapped image view
     */
    [[nodiscard]] ImageViewID map(ImageID image, vk::ImageView view);

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
     * @brief Maps an image view handle to an existing image view ID
     *
     * @param id Existing image view ID for mapping to
     * @param view Image view handle for mapping or VK_NULL_HANDLE
     *
     * @warning The image view handle must be created for a persistent image
     *
     * @warning During executing the graph, make sure you have created each image view
     * for the same images that are mapped for respective IDs
     */
    void map(ImageViewID id, vk::ImageView view);

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

    /**
     * @brief Gets the image view handle mapped to an image view ID
     *
     * @param id Image view ID to get handle mapped to
     * @return Image view handle mapped to the specified ID or VK_NULL_HANDLE
     *
     * @note During an execution, you can access only to the views that refer to the images
     * the pass reads or writes. Access to unallowed views occurs to assert failure
     */
    [[nodiscard]] vk::ImageView at(ImageViewID id) const;

  private:
    void bind_resources(detail::GraphResources const &resources) noexcept;

    void bind_resource_filters(std::span<ImageID const> images, std::span<BufferID const> buffers,
                               std::span<ImageViewID const> imageViews) noexcept;

    friend class GraphExecutor;

    std::vector<vk::Image> m_persistentImages;
    std::vector<vk::Buffer> m_persistentBuffers;
    std::vector<std::pair<ImageID, vk::ImageView>> m_persistentImageViews;

    std::span<ImageID const> m_allowedImages;
    std::span<BufferID const> m_allowedBuffers;
    std::span<ImageViewID const> m_allowedImageViews;

    detail::GraphResources const *m_transientResources = nullptr;
};
} // namespace caldera
