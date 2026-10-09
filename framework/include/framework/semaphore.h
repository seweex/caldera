#pragma once

#include <memory>
#include <vulkan/vulkan.hpp>

namespace caldera::framework
{
    struct Semaphore {
        vk::Semaphore semaphore;
        uint64_t timeline;
    };

    class SemaphoreFactory
    {
    public:
        SemaphoreFactory(vk::Device device);
        ~SemaphoreFactory() noexcept;

        SemaphoreFactory(SemaphoreFactory const&) = delete;
        SemaphoreFactory& operator=(SemaphoreFactory const&) = delete;

        [[nodiscard]] Semaphore create_semaphore(bool timeline = true);
        void destroy(Semaphore const& semaphore) noexcept;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
