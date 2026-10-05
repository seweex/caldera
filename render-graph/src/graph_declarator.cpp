#include <caldera/fwd.h>
#include <caldera/graph_declarator.h>
#include <caldera/graph_ir.h>
#include "detail/graph_declarator.h"

#include "caldera/graph_allocator.h"
#include "caldera/pass_declarator.h"
#include "detail/graph_ir.h"
#include "detail/pass_declarator.h"

namespace
{
    template <class IdTy>
    [[nodiscard]] IdTy make_transient(auto& list, auto const& description)
    {
        auto const index = static_cast<uint32_t>(list.size());
        list.push_back(description);

        return IdTy{ 1, index };
    }

    void append_versions(
        auto& table,
        caldera::FamilyType const family,
        auto const& accesses,
        uint32_t const passIdx)
    {
        for (auto const& [version, usage, kind] : accesses)
        {
            auto& list = table[version];







            if (auto const reqSize = 1u + std::max<uint32_t>(passIdx, version.version + 1u);
                reqSize > list.size())
            {
                using DescTy = typename std::remove_cvref_t<decltype(list)>::value_type;

                DescTy nullDesc {};
                nullDesc.writing.passIdx = UINT32_MAX;

                list.resize(reqSize, nullDesc);
            }

            if (kind == caldera::detail::AccessKind::read ||
                kind == caldera::detail::AccessKind::read_write)
            {
                auto& readings = list[version.version].readings;

                assert(readings.empty() || readings.back().passIdx < passIdx);
                readings.emplace_back(family, usage, passIdx);
            }

            if (kind == caldera::detail::AccessKind::write ||
                kind == caldera::detail::AccessKind::read_write)
            {
                auto& writing = list[1u + version.version].writing;

                assert(writing.passIdx == UINT32_MAX);
                writing.family = family;
                writing.usage = usage;
                writing.passIdx = passIdx;
            }
        }
    }
}

namespace caldera
{
    /* Pass Attorney */

    uint32_t GraphDeclarator::PassAttorney::emplace_pass(
        GraphDeclarator& decl,
        std::string_view const name,
        FamilyType const type)
    {
        auto& passList = decl.m_impl->passes;
        auto const passIdx = static_cast<uint32_t>(passList.size());

        passList.emplace_back(std::string{ name }, type, nullptr);
        return passIdx;
    }

    void GraphDeclarator::PassAttorney::set_pass_callback(
        GraphDeclarator &decl,
        uint32_t const passIdx,
        std::function<void(ResourceMap const&, vk::CommandBuffer)> &&fn)
    {
        decl.m_impl->passes[passIdx].callback = std::move(fn);
    }

    template <class TagTy, class UsageTy>
    BasicResourceVersion<TagTy> GraphDeclarator::PassAttorney::push_reading(
        GraphDeclarator &decl,
        uint32_t const passIdx,
        BasicResourceVersion<TagTy> const resource,
        UsageTy const usage)
    {
        auto& impl = *decl.m_impl;
        auto& versionList = std::invoke([&] {
            if constexpr (std::same_as<TagTy, ImageTag>)
                return std::ref(impl.imageVersions[resource]);
            else
                return std::ref(impl.bufferVersions[resource]);
        }).get();

        if (versionList.empty())
        {
            assert(resource.is_transient);
            using ToucherTy = std::remove_cvref_t<decltype(versionList)>::value_type::Toucher;

            ToucherTy constexpr externalToucher = { FamilyType::unknown, {}, UINT32_MAX };
            versionList.emplace_back(externalToucher);
        }

        assert(versionList.size() > resource.version);

        auto& passList = impl.passes;
        auto const passFamily = passList[passIdx].family;

        versionList[resource.version].readings.emplace_back(passFamily, usage, passIdx);
        return resource;
    }

    template<class TagTy, class UsageTy>
    BasicResourceVersion<TagTy> GraphDeclarator::PassAttorney::push_writing(
        GraphDeclarator &decl,
        uint32_t const passIdx,
        BasicResourceVersion<TagTy> const resource,
        UsageTy const usage)
    {
        auto& impl = *decl.m_impl;
        auto& versionList = std::invoke([&] {
            if constexpr (std::same_as<TagTy, ImageTag>)
                return std::ref(impl.imageVersions[resource]);
            else
                return std::ref(impl.bufferVersions[resource]);
        }).get();

        auto& passList = impl.passes;
        auto const passFamily = passList[passIdx].family;

        if (versionList.empty())
        {
            using ToucherTy = std::remove_cvref_t<decltype(versionList)>::value_type::Toucher;

            ToucherTy constexpr externalToucher = { FamilyType::unknown, {}, UINT32_MAX };
            versionList.emplace_back(externalToucher);
        }

        assert(versionList.size() > resource.version);
        auto const versionIdx = static_cast<uint32_t>(versionList.size());

        auto& newVersion = versionList.emplace_back();
        newVersion.writing.family = passFamily;
        newVersion.writing.usage = usage;
        newVersion.writing.passIdx = passIdx;

        auto result = resource;
        result.version = versionIdx;

        return result;
    }

    template ImageVersion
    GraphDeclarator::PassAttorney::push_reading<ImageTag, ImageUsage>(
        GraphDeclarator &decl, uint32_t passIdx, ImageVersion resource, ImageUsage usage);

    template BufferVersion
    GraphDeclarator::PassAttorney::push_reading<BufferTag, BufferUsage>(
        GraphDeclarator &decl, uint32_t passIdx, BufferVersion resource, BufferUsage usage);

    template ImageVersion
    GraphDeclarator::PassAttorney::push_writing<ImageTag, ImageUsage>(
        GraphDeclarator &decl, uint32_t passIdx, ImageVersion resource, ImageUsage usage);

    template BufferVersion
    GraphDeclarator::PassAttorney::push_writing<BufferTag, BufferUsage>(
        GraphDeclarator &decl, uint32_t passIdx, BufferVersion resource, BufferUsage usage);

    /* Graph Declarator */

    GraphDeclarator::GraphDeclarator() :
        m_impl(std::make_unique<Impl>())
    {}

    GraphDeclarator::~GraphDeclarator() noexcept = default;

    GraphDeclarator::GraphDeclarator(GraphDeclarator &&) noexcept = default;
    GraphDeclarator & GraphDeclarator::operator=(GraphDeclarator &&) noexcept = default;

    ImageID GraphDeclarator::transient_image(ImageDescription const& description) {
        return ::make_transient<ImageID>(m_impl->transientImages, description);
    }

    BufferID GraphDeclarator::transient_buffer(BufferDescription const& description) {
        return ::make_transient<BufferID>(m_impl->transientBuffers, description);
    }

    bool GraphDeclarator::is_complete() const noexcept {
        return !m_impl->passes.empty();
    }

    GraphIR GraphDeclarator::bake(vk::Device const device) const
    {
        auto const& impl = *m_impl;

        return GraphIR::Factory::create(
            device,
            impl.passes,
            impl.transientImages,
            impl.transientBuffers,
            impl.imageVersions,
            impl.bufferVersions);
    }
}
