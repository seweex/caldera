#include <cassert>

#include <caldera/resource_id.hpp>
#include <caldera/resource_state_manager.hpp>
#include <caldera/structs.hpp>

namespace {
void dump(auto &list, auto const id, auto const &state) {
    assert(!id.isTransient && "can not dump for a transient resources");

    if (list.size() <= id.index)
        list.reserve(id.index + 1);

    list[id.index] = state;
}

[[nodiscard]] auto get(auto const &list, auto const id) {
    assert(!id.isTransient && "can not get for a transient resources");
    return list[id.index];
}
} // namespace

namespace caldera {
void ResourceStateManager::dump(ImageID const id, ImageLastState const &state) { ::dump(m_imageTouches, id, state); }

void ResourceStateManager::dump(BufferID const id, BufferLastState const &state) { ::dump(m_bufferTouches, id, state); }

ImageLastState ResourceStateManager::get_last_state(ImageID const id) const { return ::get(m_imageTouches, id); }

BufferLastState ResourceStateManager::get_last_state(BufferID const id) const { return ::get(m_bufferTouches, id); }
} // namespace caldera
