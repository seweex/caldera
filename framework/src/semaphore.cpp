#include <framework/semaphore.h>

#include "detail/utility.h"

namespace
{
    [[nodiscard]] vk::Semaphore create_semaphore(
        vk::Device const device,
        bool const timeline)
    {
        vk::StructureChain const createInfo {
            vk::SemaphoreCreateInfo{},
            vk::SemaphoreTypeCreateInfo{
                timeline ? vk::SemaphoreType::eTimeline : vk::SemaphoreType::eBinary,
                0
            }
        };

        vk::Semaphore semaphore;

        auto const rCode = device.createSemaphore(&createInfo.get<>(), nullptr, &semaphore);
        caldera::framework::validate_result(rCode, "failed to create semaphore");

        return semaphore;
    }

}

namespace caldera::framework
{
    struct SemaphoreFactory::Impl
    {
        vk::Device device;
    };

    SemaphoreFactory::SemaphoreFactory(vk::Device const device) :
        m_impl(std::make_unique<Impl>(device))
    {}

    SemaphoreFactory::~SemaphoreFactory() noexcept = default;

    Semaphore SemaphoreFactory::create_semaphore(bool const timeline) {
        return { ::create_semaphore(m_impl->device, timeline), 0 };
    }

    void SemaphoreFactory::destroy(Semaphore const &semaphore) noexcept {
        m_impl->device.destroySemaphore(semaphore.semaphore);
    }
}
