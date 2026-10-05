#pragma once

#include <memory>
#include <functional>
#include <vulkan/vulkan.hpp>
#include "fwd.h"

namespace caldera
{
    class PassDeclarator
    {
    public:
        PassDeclarator(
            std::string_view passName,
            FamilyType family,
            GraphDeclarator& graphDeclarator);

        ~PassDeclarator() noexcept;

        PassDeclarator(PassDeclarator&&) noexcept;
        PassDeclarator& operator=(PassDeclarator&&) noexcept;

        PassDeclarator(PassDeclarator const&) = delete;
        PassDeclarator& operator=(PassDeclarator const&) = delete;

        ImageVersion read(ImageVersion resource, ImageUsage usage);
        ImageVersion read(ImageID resource, ImageUsage usage);

        BufferVersion read(BufferVersion resource, BufferUsage usage);
        BufferVersion read(BufferID resource, BufferUsage usage);

        [[nodiscard]] ImageVersion write(ImageVersion resource, ImageUsage usage);
        [[nodiscard]] ImageVersion write(ImageID resource, ImageUsage usage);

        [[nodiscard]] BufferVersion write(BufferVersion resource, BufferUsage usage);
        [[nodiscard]] BufferVersion write(BufferID resource, BufferUsage usage);

        [[nodiscard]] ImageVersion read_write(ImageVersion resource, ImageUsage usage);
        [[nodiscard]] ImageVersion read_write(ImageID resource, ImageUsage usage);

        [[nodiscard]] BufferVersion read_write(BufferVersion resource, BufferUsage usage);
        [[nodiscard]] BufferVersion read_write(BufferID resource, BufferUsage usage);

        void set_callback(std::function<void(ResourceMap const&, vk::CommandBuffer)> callback);

        struct AccessListAttorney;
        struct PassAttorney;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
