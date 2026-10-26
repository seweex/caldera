#pragma once

#include <cstdint>
#include <utility>
#include <concepts>
#include <string_view>
#include <functional>
#include <vulkan/vulkan.hpp>

#include <caldera/structs.hpp>
#include <caldera/pass_declarator.hpp>

namespace caldera
{
    /**
     * @brief Class for describing what render graph does and what
     * resources it accesses
     */
    class GraphDeclarator
    {
    public:
        /**
         * @brief Declares an image that exists inside the graph only
         *
         * @param description Description of a transient image
         * @return ID of the newly declared transient image
         *
         * @note Transient resources don't save its state across execution
         */
        [[nodiscard]] ImageID transient_image(ImageDescription const& description);

        /**
         * @brief Declares a buffer that exists inside the graph only
         *
         * @param description Description of a transient buffer
         * @return ID of the newly declared transient buffer
         *
         * @note Transient resources don't save its state across execution
         */
        [[nodiscard]] BufferID transient_buffer(BufferDescription const& description);

        /**
         * @brief Adds pass to the render graph currently being described
         *
         * @tparam UserData Type of user data that will be provided to the callback
         *
         * @param passName Name of the pass currently being added
         * @param passType Type of the pass currently being added
         * @param setup Invocable object for setting up the PassDeclarator and UserData that
         * will be invoked immediately
         * @param callback Invocable object for executing during a graph execution that writes
         * to a vk::CommandBuffer object
         *
         * @return Reference to the current GraphDeclarator
         *
         * @note UserData is copied to the pass data and stored there through the graph is
         * destroyed. The user data saves its state across execution of the graph
         */
        template <class UserData>
            requires (std::default_initializable<UserData> && std::copy_constructible<UserData>)
        GraphDeclarator& add_pass(
            std::string_view const passName,
            FamilyType const passType,
            std::function<void(PassDeclarator&, UserData&)> const& setup,
            std::function<void(vk::CommandBuffer, ResourceMap const&, UserData&)> callback)
        {
            auto const passIdx = static_cast<uint32_t>(m_ir.passes.size());
            PassDeclarator passDeclarator{ passName, passType, passIdx, m_ir.imageVersions, m_ir.bufferVersions };

            UserData usrData{};
            std::invoke(setup, passDeclarator, usrData);

            m_ir.passes.emplace_back(std::move(passDeclarator).extract(
                [cb = std::move(callback), udt = std::move(usrData)]
                (vk::CommandBuffer const cmd, ResourceMap const& rMap) mutable
                { std::invoke(cb, cmd, rMap, udt); }));

            return *this;
        }

        /**
         * @brief Adds pass with no user data to the render graph currently being described
         *
         * @param passName Name of the pass currently being added
         * @param passType Type of the pass currently being added
         * @param setup Invocable object for setting up the PassDeclarator and UserData that
         * will be invoked immediately
         * @param callback Invocable object for executing during a graph execution that writes
         * to a vk::CommandBuffer object
         *
         * @return Reference to the current GraphDeclarator
         */
        GraphDeclarator& add_pass(
            std::string_view const passName,
            FamilyType const passType,
            std::function<void(PassDeclarator&)> const& setup,
            std::function<void(vk::CommandBuffer, ResourceMap const&)> callback)
        {
            auto const passIdx = static_cast<uint32_t>(m_ir.passes.size());
            PassDeclarator passDeclarator{ passName, passType, passIdx, m_ir.imageVersions, m_ir.bufferVersions };

            std::invoke(setup, passDeclarator);

            m_ir.passes.emplace_back(
                std::move(passDeclarator).extract(std::move(callback)));

            return *this;
        }

        /**
         * @brief Get described graph representation
         * @return Graph intermediate representation object
         */
        [[nodiscard]] GraphIR extract() const &;

        /**
         * @brief Retrieves described graph representation and clears the GraphDeclarator
         * @return Graph intermediate representation object
         */
        [[nodiscard]] GraphIR extract() && noexcept;

    private:
        GraphIR m_ir;
    };
}
