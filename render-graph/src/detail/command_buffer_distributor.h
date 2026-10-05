#pragma once

#include <cstdint>
#include <array>

#include <caldera/command_buffer_distributor.h>
#include <caldera/structs.h>

namespace caldera::detail
{
    struct CommandFamily
    {
        std::vector<vk::CommandBuffer> buffers;
        uint32_t activeCount;
    };
}

namespace caldera
{
    struct CommandBufferDistributor::Impl
    {
        [[nodiscard]] detail::CommandFamily& get_command(FamilyType type) noexcept;
        [[nodiscard]] vk::CommandBuffer take_buffer(FamilyType type) noexcept;

        std::array<detail::CommandFamily, 3> commands;
    };

    struct CommandBufferDistributor::ConsumerAttorney
    {
        [[nodiscard]] static vk::CommandBuffer take_buffer(
            CommandBufferDistributor& distributor, FamilyType type) noexcept;
    };
}
