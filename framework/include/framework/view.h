#pragma once

#include <memory>
#include <vulkan/vulkan.hpp>

namespace caldera::framework
{
    struct ImageView {
        vk::ImageView view;
    };

    class ImageViewFactory
    {
    public:
        ImageViewFactory(vk::Device device);
        ~ImageViewFactory() noexcept;

        ImageViewFactory(ImageViewFactory const&) = delete;
        ImageViewFactory& operator=(ImageViewFactory const&) = delete;

        [[nodiscard]] ImageView create_image_view(vk::ImageViewCreateInfo const& info);
        void destroy(ImageView view) noexcept;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
