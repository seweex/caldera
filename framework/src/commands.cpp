#include <framework/commands.h>

#include "detail/utility.h"

namespace
{
    [[nodiscard]] vk::CommandPool create_pool(
        vk::Device const device,
        uint32_t const family)
    {
        vk::CommandPoolCreateInfo info {};
        info.flags = vk::CommandPoolCreateFlagBits::eTransient;
        info.queueFamilyIndex = family;

        vk::CommandPool pool;

        auto const rCode = device.createCommandPool(&info, nullptr, &pool);
        caldera::framework::validate_result(rCode, "failed to create command pool");

        return pool;
    }

    [[nodiscard]] std::vector<vk::CommandBuffer> create_command_buffers(
        vk::Device const device,
        vk::CommandPool const pool,
        uint32_t const count)
    {
        vk::CommandBufferAllocateInfo info {};
        info.commandPool = pool;
        info.commandBufferCount = count;
        info.level = vk::CommandBufferLevel::ePrimary;

        std::vector<vk::CommandBuffer> commandBuffers;
        commandBuffers.resize(count, nullptr);

        auto const rCode = device.allocateCommandBuffers(&info, &commandBuffers[0]);
        caldera::framework::validate_result(rCode, "failed to allocate command buffers");

        return commandBuffers;
    }
}

namespace caldera::framework
{
    struct CommandFactory::Impl
    {
        vk::Device device;
    };

    CommandFactory::CommandFactory(vk::Device const device) :
        m_impl(std::make_unique<Impl>(device))
    {}

    CommandFactory::~CommandFactory() noexcept = default;

    CommandBuffers CommandFactory::create_command_buffers(
        uint32_t const family, uint32_t const count)
    {
        auto const device = m_impl->device;

        auto const pool = ::create_pool(device, family);
        auto buffers = ::create_command_buffers(device, pool, count);

        return { pool, std::move(buffers) };
    }

    void CommandFactory::destroy(CommandBuffers const &buffers) noexcept {
        m_impl->device.destroyCommandPool(buffers.pool);
    }
}
