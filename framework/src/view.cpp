#include <framework/view.h>

#include "detail/utility.h"

namespace caldera::framework
{
    struct ImageViewFactory::Impl {
        vk::Device device;
    };

    ImageViewFactory::ImageViewFactory(vk::Device const device) :
        m_impl(std::make_unique<Impl>(device))
    {}

    ImageViewFactory::~ImageViewFactory() noexcept = default;

    ImageView ImageViewFactory::create_image_view(vk::ImageViewCreateInfo const &info)
    {
        vk::ImageView view;

        auto const rCode = m_impl->device.createImageView(&info, nullptr, &view);
        validate_result(rCode, "failed to create image view");

        return { view };
    }

    void ImageViewFactory::destroy(ImageView const view) noexcept {
        m_impl->device.destroyImageView(view.view);
    }
}
