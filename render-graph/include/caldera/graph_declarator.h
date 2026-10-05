#pragma once

#include <memory>
#include <string_view>
#include <vulkan/vulkan.hpp>

#include "fwd.h"

namespace caldera
{
    class GraphDeclarator
    {
    public:
        GraphDeclarator();
        ~GraphDeclarator() noexcept;

        GraphDeclarator(GraphDeclarator&&) noexcept;
        GraphDeclarator& operator=(GraphDeclarator&&) noexcept;

        GraphDeclarator(GraphDeclarator const&) = delete;
        GraphDeclarator& operator=(GraphDeclarator const&) = delete;

        [[nodiscard]] ImageID transient_image(ImageDescription const& description);
        [[nodiscard]] BufferID transient_buffer(BufferDescription const& description);

        [[nodiscard]] bool is_complete() const noexcept;
        [[nodiscard]] GraphIR bake(vk::Device device) const;

        struct PassAttorney;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
