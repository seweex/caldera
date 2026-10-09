#include <framework/fence.h>

#include "detail/utility.h"

namespace caldera::framework
{
    struct FenceFactory::Impl {
        vk::Device device;
    };

    FenceFactory::FenceFactory(vk::Device const device) :
        m_impl(std::make_unique<Impl>(device))
    {}

    FenceFactory::~FenceFactory() noexcept = default;

    Fence FenceFactory::create_fence(bool const signaled)
    {
        vk::FenceCreateInfo info;
        info.flags = signaled ? vk::FenceCreateFlagBits::eSignaled : vk::FenceCreateFlags{};

        vk::Fence fence;

        auto const rCode = m_impl->device.createFence(&info, nullptr, &fence);
        validate_result(rCode, "failed to create a fence");

        return { fence };
    }

    void FenceFactory::destroy(Fence const fence) {
        m_impl->device.destroyFence(fence.fence);
    }
}
