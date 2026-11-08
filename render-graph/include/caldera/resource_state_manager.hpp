#pragma once

#include <caldera/fwd.hpp>
#include <vector>
#include <flat_map>

namespace caldera {
/**
 * @brief Stores vulkan resource last states across the graph
 * executions to provide the most efficient synchronization
 */
class ResourceStateManager {
  public:
    /**
     * @brief Maps an image state to the image handle
     *
     * @param image Image handle to map the last state to
     * @param state State of the specified image
     */
    void dump(vk::Image image, ImageState const &state);

    /**
     * @brief Maps a buffer state to the buffer handle
     *
     * @param buffer Buffer handle to map the last state to
     * @param state State of the specified buffer
     */
    void dump(vk::Buffer buffer, BufferState const &state);

    /**
     * @brief Gets the image state mapped to the specified image
     *
     * @param image Image handle to get the state mapped to
     * @return Last image state if was dumped, otherwise initial image state
     */
    [[nodiscard]] ImageState get_last_state(vk::Image image) const;

    /**
     * @brief Gets the buffer state mapped to the specified buffer
     *
     * @param buffer Buffer handle to get the state mapped to
     * @return Last buffer state if was dumped, otherwise initial buffer state
     */
    [[nodiscard]] BufferState get_last_state(vk::Buffer buffer) const;

  private:
    std::flat_map<vk::Image, ImageState> m_imageStates;
    std::flat_map<vk::Buffer, BufferState> m_bufferStates;
};
} // namespace caldera
