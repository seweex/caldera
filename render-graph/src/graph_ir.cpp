#include <caldera/graph_ir.h>
#include "detail/graph_ir.h"
#include "detail/structs.h"

namespace
{
    void sort_version_readings(auto& table)
    {
        using VersionTy = std::remove_cvref_t<decltype(table)>::mapped_type::value_type;

        for (auto& [resourceID, versionList] : table)
            for (auto& version : versionList)
                std::ranges::sort(version.readings, typename VersionTy::Toucher::Comparator{});
    }
}

namespace caldera
{
    /* Constructor */

    GraphIR GraphIR::Factory::create(
        vk::Device const device,
        std::vector<detail::PassDeclaration> const& passes,
        std::vector<ImageDescription> const &transientImages,
        std::vector<BufferDescription> const &transientBuffers,
        std::unordered_map<ImageID, std::vector<detail::ImageVersionDesc>> const &imageVersions,
        std::unordered_map<BufferID, std::vector<detail::BufferVersionDesc>> const &bufferVersions)
    {
        GraphIR result {};

        auto sortedImageVersions = imageVersions;
        auto sortedBufferVersions = bufferVersions;

        ::sort_version_readings(sortedImageVersions);
        ::sort_version_readings(sortedBufferVersions);

        describe_resources(result,
            device, transientImages, transientBuffers, sortedImageVersions, sortedBufferVersions);

        describe_execution(result,
            passes, sortedImageVersions, sortedBufferVersions);

        return result;
    }

    /* Graph IR */

    GraphIR::GraphIR() :
        m_impl(std::make_unique<Impl>())
    {}

    GraphIR::~GraphIR() noexcept = default;

    GraphIR::GraphIR(GraphIR &&) noexcept = default;
    GraphIR & GraphIR::operator=(GraphIR &&) noexcept = default;
}
