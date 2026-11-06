#include <cassert>

#include <caldera/resource_id.hpp>
#include <caldera/resource_state_manager.hpp>
#include <caldera/structs.hpp>

namespace {
template <class TagTy>
[[nodiscard]] constexpr auto make_unknown_state() noexcept {
    if constexpr (std::same_as<TagTy, caldera::ImageTag>)
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

template <class TagTy>
void dump(auto &list, caldera::BasicResourceID<TagTy> const id, auto const &state) {
    assert(!id.isTransient && "can not dump for a transient resources");

    if (list.size() <= id.index)
        list.resize(id.index + 1, make_unknown_state<TagTy>());

    list[id.index] = state;
}

template <class TagTy>
[[nodiscard]] auto get(auto const &list, caldera::BasicResourceID<TagTy> const id) {
    assert(!id.isTransient && "can not get for a transient resources");

    if (list.size() <= id.index)
        return make_unknown_state<TagTy>();

    return list[id.index];
}
} // namespace

namespace caldera {
void ResourceStateManager::dump(ImageID const id, ImageState const &state) { ::dump(m_imageTouches, id, state); }

void ResourceStateManager::dump(BufferID const id, BufferState const &state) { ::dump(m_bufferTouches, id, state); }

ImageState ResourceStateManager::get_last_state(ImageID const id) const { return ::get(m_imageTouches, id); }

BufferState ResourceStateManager::get_last_state(BufferID const id) const { return ::get(m_bufferTouches, id); }
} // namespace caldera
