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
    class GraphDeclarator
    {
    public:
        [[nodiscard]] ImageID transient_image(ImageDescription const& description);
        [[nodiscard]] BufferID transient_buffer(BufferDescription const& description);

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

        [[nodiscard]] GraphIR extract() const & noexcept;
        [[nodiscard]] GraphIR extract() && noexcept;

    private:
        GraphIR m_ir;
    };
}
