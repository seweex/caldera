#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <functional>
#include <vulkan/vulkan.hpp>

#include <caldera/structs.hpp>

namespace caldera
{
    /**
     * @brief Class for simple declaration of a render graph pass
     */
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
        /**
         * @brief Processes reading of the specific image version
         *
         * @param resource Image version to read
         * @param usage Mode the image will be used in
         *
         * @note Reading of a version doesn't create a new one
         * @note You can't read the zero version (initial state)
         * of a transient resource
         */
        void read(ImageVersion resource, ImageUsage usage);

        /**
         * @brief Processes reading of the specific buffer version
         *
         * @param resource Buffer version to read
         * @param usage Mode the buffer will be used in
         *
         * @note Reading of a version doesn't create a new one
         * @note You can't read the zero version (initial state)
         * of a transient resource
         */
        void read(BufferVersion resource, BufferUsage usage);

        /**
         * @brief Processes writing over the specific image version
         *
         * @param resource Image version to overwrite
         * @param usage Mode the image will be used in
         *
         * @return New version of the overwritten image
         *
         * @note Writing returns a new version of the resource. You can
         * read a version many times, but you can overwrite it only once
         */
        ImageVersion write(ImageVersion resource, ImageUsage usage);

        /**
         * @brief Processes writing over the specific buffer version
         *
         * @param resource Buffer version to overwrite
         * @param usage Mode the buffer will be used in
         *
         * @return New version of the overwritten buffer
         *
         * @note Writing returns a new version of the resource. You can
         * read a version many times, but you can overwrite it only once
         */
        BufferVersion write(BufferVersion resource, BufferUsage usage);

        /**
         * @brief Processes reading of and writing over the specific image version
         *
         * @param resource Image version to read and overwrite
         * @param usage Mode the image will be used in
         *
         * @return New version of the overwritten image
         *
         * @note Writing returns a new version of the resource. You can
         * read a version many times, but you can overwrite it only once
         *
         * @note You can't read the zero version (initial state)
         * of a transient resource
         */
        ImageVersion read_write(ImageVersion resource, ImageUsage usage);

        /**
         * @brief Processes reading of and writing over the specific buffer version
         *
         * @param resource Buffer version to read and overwrite
         * @param usage Mode the buffer will be used in
         *
         * @return New version of the overwritten buffer
         *
         * @note Writing returns a new version of the resource. You can
         * read a version many times, but you can overwrite it only once
         *
         * @note You can't read the zero version (initial state)
         * of a transient resource
         */
        BufferVersion read_write(BufferVersion resource, BufferUsage usage);

    private:
        std::string m_name;
        FamilyType m_type;
        uint32_t m_index;

        detail::ImageVersionTable* m_imageTable;
        detail::BufferVersionTable* m_bufferTable;
    };
}
