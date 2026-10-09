#pragma once

#include <vector>
#include <vulkan/vulkan.hpp>

namespace caldera::framework
{
    struct CommandBuffers {
        vk::CommandPool pool;
        std::vector<vk::CommandBuffer> buffers;
    };

    class CommandFactory
    {
    public:
        CommandFactory(vk::Device device);
        ~CommandFactory() noexcept;

        CommandFactory(CommandFactory const&) = delete;
        CommandFactory& operator=(CommandFactory const&) = delete;

        [[nodiscard]] CommandBuffers create_command_buffers(uint32_t family, uint32_t count);
        void destroy(CommandBuffers const& buffers) noexcept;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}