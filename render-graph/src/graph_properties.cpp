#include <caldera/graph_properties.hpp>

#include <caldera/detail/access_resolver.hpp>
#include <caldera/render_graph.hpp>
#include <caldera/resource_id.hpp>
#include <caldera/structs.hpp>

namespace caldera {

GraphProperties::GraphProperties(RenderGraph const &graph) : m_graph(graph) {}

uint32_t GraphProperties::get_pass_count() const noexcept {
    return static_cast<uint32_t>(m_graph.m_compiled.compiledPasses.size());
}

std::vector<FamilyPassCount> GraphProperties::get_pass_counts_per_families() const noexcept {
    std::vector<FamilyPassCount> result;
    result.reserve(3);

    for (auto const &pass : m_graph.m_compiled.compiledPasses)
        if (auto const iter = std::ranges::find(result, pass.familyIdx, &FamilyPassCount::family);
            iter != result.end()) {
            ++iter->passCount;
        } else
            result.emplace_back(pass.familyIdx, 1);

    return result;
}

SemaphoreSyncPoint GraphProperties::find_image_acquire_sync_point(ImageID const swapchainImageID) const {
    auto const &firstTouch = m_graph.m_compiled.trackedImages.at(swapchainImageID).firstTouch;
    return SemaphoreSyncPoint{.passIdx = firstTouch.passIdx, .flags = firstTouch.stages};
}

SemaphoreSyncPoint GraphProperties::find_render_finished_sync_point(ImageID const swapchainImageID) const {
    auto const &lastTouch = m_graph.m_compiled.trackedImages.at(swapchainImageID).lastTouch;
    return SemaphoreSyncPoint{.passIdx = lastTouch.passIdx, .flags = lastTouch.stages};
}

} // namespace caldera