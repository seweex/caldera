#pragma once

#include <vector>

#include <caldera/graph_ir.h>
#include "detail/structs.h"

namespace caldera
{
    struct GraphIR::Impl
    {
        detail::GraphResourceIR resources;
        detail::GraphExecutionIR execution;
    };

    struct GraphIR::Factory
    {
    private:
        static void describe_resources(
            GraphIR& ir,
            vk::Device device,
            std::vector<ImageDescription> const& transientImages,
            std::vector<BufferDescription> const& transientBuffers,
            std::unordered_map<ImageID, std::vector<detail::ImageVersionDesc>> const& imageVersions,
            std::unordered_map<BufferID, std::vector<detail::BufferVersionDesc>> const& bufferVersions);

        static void describe_execution(
            GraphIR& ir,
            std::vector<detail::PassDeclaration> const& passes,
            std::unordered_map<ImageID, std::vector<detail::ImageVersionDesc>> const& imageVersions,
            std::unordered_map<BufferID, std::vector<detail::BufferVersionDesc>> const& bufferVersions);

    public:
        [[nodiscard]] static GraphIR create(
            vk::Device device,
            std::vector<detail::PassDeclaration> const& passes,
            std::vector<ImageDescription> const& transientImages,
            std::vector<BufferDescription> const& transientBuffers,
            std::unordered_map<ImageID, std::vector<detail::ImageVersionDesc>> const& imageVersions,
            std::unordered_map<BufferID, std::vector<detail::BufferVersionDesc>> const& bufferVersions);
    };

    struct GraphIR::ExecutionAttorney
    {
        [[nodiscard]] static detail::GraphExecutionIR const&
        get_execution(GraphIR const& ir) noexcept;
    };

    struct GraphIR::ResourcesAttorney
    {
        [[nodiscard]] static detail::GraphResourceIR const&
        get_resources(GraphIR const& ir) noexcept;
    };
}