#pragma once

#include <vector>
#include <caldera/fwd.hpp>

namespace caldera
{
    class ResourceStateManager
    {
    public:
        void dump(ImageID id, ImageLastState const& state);
        void dump(BufferID id, BufferLastState const& state);

        [[nodiscard]] ImageLastState get_last_state(ImageID id) const;
        [[nodiscard]] BufferLastState get_last_state(BufferID id) const;

    private:
        std::vector<ImageLastState> m_imageTouches;
        std::vector<BufferLastState> m_bufferTouches;
    };
}
