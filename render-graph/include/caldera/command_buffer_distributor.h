#pragma once

#include <memory>
#include <span>
#include <vulkan/vulkan.hpp>

#include "fwd.h"

namespace caldera
{
    class CommandBufferDistributor
    {
    public:
        CommandBufferDistributor();
        ~CommandBufferDistributor() noexcept;

        CommandBufferDistributor(CommandBufferDistributor&&) noexcept;
        CommandBufferDistributor& operator=(CommandBufferDistributor&&) noexcept;

        CommandBufferDistributor(CommandBufferDistributor const&) = delete;
        CommandBufferDistributor& operator=(CommandBufferDistributor const&) = delete;

        void feed(FamilyType family, std::span<vk::CommandBuffer const> buffers);
        void reset(FamilyType family) noexcept;

        struct ConsumerAttorney;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
