#pragma once

#include <caldera/fwd.hpp>
#include <vector>

namespace caldera {
/**
 * @brief Stores vulkan resource last states across the graph
 * executions to provide the most efficient synchronization
 */
class ResourceStateManager {
  public:
    /**
     * @brief Maps an image state to the image ID
     *
     * @param id Image ID to map the last state to
     * @param state State of the specified image
     */
    void dump(ImageID id, ImageState const &state);

    /**
     * @brief Maps a buffer state to the buffer ID
     *
     * @param id Buffer ID to map the last state to
     * @param state State of the specified buffer
     */
    void dump(BufferID id, BufferState const &state);

    /**
     * @brief Gets the image state mapped to the specified image
     *
     * @param id Image ID to get the state mapped to
     * @return Last image state if was dumped, otherwise initial image state
     */
    [[nodiscard]] ImageState get_last_state(ImageID id) const;

    /**
     * @brief Gets the buffer state mapped to the specified buffer
     *
     * @param id Buffer ID to get the state mapped to
     * @return Last buffer state if was dumped, otherwise initial buffer state
     */
    [[nodiscard]] BufferState get_last_state(BufferID id) const;

  private:
    std::vector<ImageState> m_imageTouches;
    std::vector<BufferState> m_bufferTouches;
};
} // namespace caldera
