#include <caldera/command_buffer_distributor.h>

#include "detail/command_buffer_distributor.h"
#include "detail/utility.h"

namespace caldera
{
    /* Impl */

    detail::CommandFamily& CommandBufferDistributor::Impl::get_command(FamilyType const type) noexcept
    {
        auto const index = static_cast<uint32_t>(detail::family_to_index(type));
        return commands[index];
    }

    vk::CommandBuffer CommandBufferDistributor::Impl::take_buffer(FamilyType const type) noexcept
    {
        auto& source = get_command(type);
        assert(source.buffers.size() > source.activeCount);

        return source.buffers[source.activeCount++];
    }

    /* Consumer Attorney */

    vk::CommandBuffer CommandBufferDistributor::ConsumerAttorney::take_buffer(
        CommandBufferDistributor& distributor,
        FamilyType const type) noexcept
    {
        return distributor.m_impl->take_buffer(type);
    }

    /* Main */

    CommandBufferDistributor::CommandBufferDistributor() :
        m_impl(std::make_unique<Impl>())
    {
        m_impl->commands.fill({});
    }

    CommandBufferDistributor::~CommandBufferDistributor() noexcept = default;

    CommandBufferDistributor::CommandBufferDistributor(CommandBufferDistributor &&) noexcept = default;
    CommandBufferDistributor & CommandBufferDistributor::operator=(CommandBufferDistributor &&) noexcept = default;

    void CommandBufferDistributor::feed(
        FamilyType const family,
        std::span<vk::CommandBuffer const> const buffers)
    {
        auto& target = m_impl->get_command(family).buffers;
        target.insert(target.end(), buffers.begin(), buffers.end());
    }

    void CommandBufferDistributor::reset(FamilyType const family) noexcept {
        m_impl->get_command(family).activeCount = 0;
    }
}
