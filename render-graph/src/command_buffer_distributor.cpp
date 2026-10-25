#include <caldera/command_buffer_distributor.hpp>

namespace caldera
{
    void CommandBufferDistributor::feed(
        uint32_t const family,
        std::span<vk::CommandBuffer const> buffers)
    {
        if (m_commandFamilies.size() <= family)
            m_commandFamilies.resize(family + 1);

        auto& target = m_commandFamilies[family].buffers;
        target.insert(target.end(), buffers.begin(), buffers.end());
    }

    void CommandBufferDistributor::reset(uint32_t const family) noexcept {
        m_commandFamilies[family].activeCount = 0;
    }

    vk::CommandBuffer CommandBufferDistributor::take_buffer(uint32_t const family) {
        auto& commands = m_commandFamilies[family];
        return commands.buffers[commands.activeCount++];
    }
}
