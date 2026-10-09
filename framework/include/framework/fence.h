#pragma once

#include <memory>
#include <vulkan/vulkan.hpp>

namespace caldera::framework
{
    struct Fence {
        vk::Fence fence;
    };

    class FenceFactory
    {
    public:
        FenceFactory(vk::Device device);
        ~FenceFactory() noexcept;

        FenceFactory(FenceFactory const&) = delete;
        FenceFactory& operator=(FenceFactory const&) = delete;

        [[nodiscard]] Fence create_fence(bool signaled = false);
        void destroy(Fence fence);

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}