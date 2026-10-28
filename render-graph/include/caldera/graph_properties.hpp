#pragma once

#include <cstdint>
#include <vector>

#include <caldera/fwd.hpp>

namespace caldera {
/// @brief Retrieves some properties of a render graph
class GraphProperties {
  public:
    GraphProperties(RenderGraph const &graph);

    /// @result Returns the total count of passes
    [[nodiscard]] uint32_t get_pass_count() const noexcept;

    /**
     * @brief Return pass count for each queue family that wes used in the render graph
     * @return List of pair (familyIdx, passCount)
     */
    [[nodiscard]] std::vector<FamilyPassCount> get_pass_counts_per_families() const noexcept;

    /**
     * @brief Returns the index of a pass and the stage mask for image acquire semaphore
     *
     * @param swapchainImageID Image ID of considered swapchain image
     * @return Sync point for image acquire semaphore
     */
    [[nodiscard]] SemaphoreSyncPoint find_image_acquire_sync_point(ImageID swapchainImageID) const;

    /**
     * @brief Returns the index of a pass and the stage mask for render finished semaphore
     *
     * @param swapchainImageID Image ID of considered swapchain image
     * @return Sync point for render finished semaphore
     */
    [[nodiscard]] SemaphoreSyncPoint find_render_finished_sync_point(ImageID swapchainImageID) const;

  private:
    RenderGraph const &m_graph;
};
} // namespace caldera