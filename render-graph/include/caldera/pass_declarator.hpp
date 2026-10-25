#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <functional>
#include <vulkan/vulkan.hpp>

#include <caldera/structs.hpp>

namespace caldera
{
    class PassDeclarator
    {
        PassDeclarator(
            std::string_view name,
            FamilyType family,
            uint32_t index,
            detail::ImageVersionTable& imageTable,
            detail::BufferVersionTable& bufferTable);

        [[nodiscard]] detail::PassDeclaration extract(
            std::function<void(vk::CommandBuffer, ResourceMap const&)>&&) && noexcept;

        friend class GraphDeclarator;

    public:
        void read(ImageVersion resource, ImageUsage usage);
        void read(BufferVersion resource, BufferUsage usage);

        ImageVersion write(ImageVersion resource, ImageUsage usage);
        BufferVersion write(BufferVersion resource, BufferUsage usage);

        ImageVersion read_write(ImageVersion resource, ImageUsage usage);
        BufferVersion read_write(BufferVersion resource, BufferUsage usage);

    private:
        std::string m_name;
        FamilyType m_type;
        uint32_t m_index;

        detail::ImageVersionTable* m_imageTable;
        detail::BufferVersionTable* m_bufferTable;
    };
}
