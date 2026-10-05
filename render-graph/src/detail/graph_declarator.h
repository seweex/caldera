#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <vulkan/vulkan.hpp>

#include <caldera/graph_declarator.h>
#include "detail/structs.h"

namespace caldera
{
    struct GraphDeclarator::PassAttorney
    {
        [[nodiscard]] static uint32_t emplace_pass(
            GraphDeclarator& decl, std::string_view name, FamilyType type);

        static void set_pass_callback(
            GraphDeclarator& decl,
            uint32_t passIdx,
            std::function<void(ResourceMap const&, vk::CommandBuffer)>&& fn);

        template <class TagTy, class UsageTy>
        [[nodiscard]] static BasicResourceVersion<TagTy>
        push_reading(
            GraphDeclarator& decl,
            uint32_t passIdx,
            BasicResourceVersion<TagTy> resource,
            UsageTy usage);

        template <class TagTy, class UsageTy>
        [[nodiscard]] static BasicResourceVersion<TagTy>
        push_writing(
            GraphDeclarator& decl,
            uint32_t passIdx,
            BasicResourceVersion<TagTy> resource,
            UsageTy usage);
    };

    struct GraphDeclarator::Impl
    {
        std::vector<ImageDescription> transientImages;
        std::vector<BufferDescription> transientBuffers;

        std::vector<detail::PassDeclaration> passes;

        std::unordered_map<ImageID, std::vector<detail::ImageVersionDesc>> imageVersions;
        std::unordered_map<BufferID, std::vector<detail::BufferVersionDesc>> bufferVersions;
    };
}