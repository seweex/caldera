#include <cassert>

#include <caldera/resource_id.hpp>
#include <caldera/resource_state_manager.hpp>
#include <caldera/structs.hpp>

namespace {
template <class HandleTy>
[[nodiscard]] constexpr auto make_unknown_state() noexcept {
    if constexpr (std::same_as<HandleTy, vk::Image>)
        return caldera::ImageState{
            .stages = vk::PipelineStageFlagBits2::eAllCommands,
            .access = vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead,
            .layout = vk::ImageLayout::eUndefined,
            .familyIdx = vk::QueueFamilyIgnored
        };
    else
        return caldera::BufferState{
            .stages = vk::PipelineStageFlagBits2::eAllCommands,
            .access = vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead,
            .familyIdx = vk::QueueFamilyIgnored
        };
}

template <class HandleTy>
[[nodiscard]] auto get(auto const &map, HandleTy const handle) {
    if (auto const iter = map.find(handle); iter != map.end())
        return iter->second;
    else
        return make_unknown_state<HandleTy>();
}
} // namespace

namespace caldera {
void ResourceStateManager::dump(vk::Image const image, ImageState const &state) { m_imageStates[image] = state; }

void ResourceStateManager::dump(vk::Buffer const buffer, BufferState const &state) { m_bufferStates[buffer] = state; }

ImageState ResourceStateManager::get_last_state(vk::Image const image) const { return ::get(m_imageStates, image); }

BufferState ResourceStateManager::get_last_state(vk::Buffer const buffer) const { return ::get(m_bufferStates, buffer); }
} // namespace caldera
