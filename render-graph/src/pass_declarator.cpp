#include <cstdint>
#include <cassert>
#include <unordered_map>

#include <caldera/structs.hpp>
#include <caldera/resource_id.hpp>
#include <caldera/detail/touch_translator.hpp>

#include <caldera/pass_declarator.hpp>

namespace
{
    template <class TagTy>
    void ensure_zero_version(auto& versionList)
    {
        if (versionList.empty()) {
            if constexpr (std::same_as<TagTy, caldera::ImageTag>)
                versionList.emplace_back(caldera::detail::undefined_image_touch);
            else
                versionList.emplace_back(caldera::detail::undefined_buffer_touch);
        }
    }

    template <class TagTy>
    void push_reading(
        auto& versionList,
        uint32_t const passIdx,
        caldera::FamilyType const family,
        caldera::BasicResourceVersion<TagTy> const resource,
        auto const usage)
    {
        ensure_zero_version<TagTy>(versionList);

        assert((!resource.id.isTransient || resource.versionIdx > 0) &&
            "can't read undefined state of transient resource");

        assert(versionList.size() >= resource.versionIdx &&
            "can't read non-existing version");

        caldera::detail::TouchTranslator translator;

        auto& version = versionList[resource.versionIdx];
        auto const touch = translator.translate_touch(
            family, usage, caldera::detail::AccessKind::read, passIdx);

        version.readingList.push_back(touch);
    }

    template <class TagTy>
    [[nodiscard]] caldera::BasicResourceVersion<TagTy> push_writing(
        auto& versionList,
        uint32_t const passIdx,
        caldera::FamilyType const family,
        caldera::BasicResourceVersion<TagTy> const resource,
        auto const usage)
    {
        ensure_zero_version<TagTy>(versionList);

        assert(versionList.size() == resource.versionIdx + 1 &&
            "can't overwrite non-previous version");

        caldera::detail::TouchTranslator translator;

        auto& version = versionList.emplace_back();
        version.writing = translator.translate_touch(
            family, usage, caldera::detail::AccessKind::write, passIdx);

        auto result = resource;
        ++result.versionIdx;

        return result;
    }

    template <class TagTy>
    [[nodiscard]] caldera::BasicResourceVersion<TagTy> push_read_write(
        auto& versionList,
        uint32_t const passIdx,
        caldera::FamilyType const family,
        caldera::BasicResourceVersion<TagTy> const resource,
        auto const usage)
    {
        ensure_zero_version<TagTy>(versionList);

        assert((!resource.id.isTransient || resource.versionIdx > 0) &&
            "can't read undefined state of transient resource");

        assert(versionList.size() == resource.versionIdx + 1 &&
            "can't overwrite non-previous version");

        caldera::detail::TouchTranslator translator;
        versionList.reserve(versionList.size() + 1);

        auto& oldVersion = versionList.back();
        auto& newVersion = versionList.emplace_back();

        auto const touch = translator.translate_touch(family, usage, caldera::detail::AccessKind::write, passIdx);

        oldVersion.readingList.emplace_back(touch);
        newVersion.writing = touch;

        auto result = resource;
        ++result.versionIdx;

        return result;
    }
}

namespace caldera
{
    PassDeclarator::PassDeclarator(
        std::string_view const name,
        FamilyType const family,
        uint32_t const index,
        detail::ImageVersionTable &imageTable,
        detail::BufferVersionTable &bufferTable)
    :
        m_name(name),
        m_type(family),
        m_index(index),
        m_imageTable(&imageTable),
        m_bufferTable(&bufferTable)
    {}

    detail::PassDeclaration PassDeclarator::extract(
        std::function<void(vk::CommandBuffer, ResourceMap const&)> && callback) && noexcept
    {
        return detail::PassDeclaration {
            .name = std::move(m_name),
            .family = m_type,
            .callback = std::move(callback),
        };
    }

    void PassDeclarator::read(ImageVersion const resource, ImageUsage const usage) {
        ::push_reading((*m_imageTable)[resource.id], m_index, m_type, resource, usage);
    }

    void PassDeclarator::read(BufferVersion const resource, BufferUsage const usage) {
        ::push_reading((*m_bufferTable)[resource.id], m_index, m_type, resource, usage);
    }

    ImageVersion PassDeclarator::write(ImageVersion const resource, ImageUsage const usage) {
        return ::push_writing((*m_imageTable)[resource.id], m_index, m_type, resource, usage);
    }

    BufferVersion PassDeclarator::write(BufferVersion const resource, BufferUsage const usage) {
        return ::push_writing((*m_bufferTable)[resource.id], m_index, m_type, resource, usage);
    }

    ImageVersion PassDeclarator::read_write(ImageVersion const resource, ImageUsage const usage) {
        return ::push_read_write((*m_imageTable)[resource.id], m_index, m_type, resource, usage);
    }

    BufferVersion PassDeclarator::read_write(BufferVersion const resource, BufferUsage const usage) {
        return ::push_read_write((*m_bufferTable)[resource.id], m_index, m_type, resource, usage);
    }
}
