#include <ranges>
#include <deque>

#include <caldera/graph_ir.h>
#include "detail/graph_ir.h"

namespace
{
    [[nodiscard]] bool conflicts(
        auto const& srcToucher,
        auto const& dstToucher) noexcept
    {
        return srcToucher.family != dstToucher.family ||
               srcToucher.usage != dstToucher.usage;
    }

    void append_tree(
        auto const& versions,
        auto& parentCounts,
        auto& childIndices)
    {
        std::vector<uint32_t> prevLayer;
        prevLayer.reserve(parentCounts.size());

        auto const bindWithPrev = [&] (uint32_t const passIdx) {
            for (auto const prevIdx : prevLayer)
                if (prevIdx != UINT32_MAX) {
                    childIndices[prevIdx].push_back(passIdx);
                    ++parentCounts[passIdx];
                }
        };

        for (auto const& [resourceID, versionList] : versions)
        {
            prevLayer.clear();

            for (auto const& version : versionList)
            {
                bindWithPrev(version.writing.passIdx);
                prevLayer = { version.writing.passIdx };

                auto readingIter = version.readings.begin();

                while (readingIter != version.readings.end())
                {
                    std::vector<uint32_t> currentLayer;

                    do {
                        currentLayer.push_back(readingIter->passIdx);
                        ++readingIter;
                    }
                    while (readingIter != version.readings.end() &&
                        !::conflicts(*readingIter, *std::prev(readingIter)));

                    for (auto const passIdx : currentLayer)
                        bindWithPrev(passIdx);

                    prevLayer = std::move(currentLayer);
                }
            }
        }
    }

    [[nodiscard]] std::vector<uint32_t>
    topological_sort(
        auto const& children,
        auto const& parentCounts)
    {
        auto const passCount = parentCounts.size();
        assert(children.size() == passCount);

        std::vector<uint32_t> remainingParents = parentCounts;
        std::vector<uint32_t> result;

        result.reserve(passCount);

        std::deque<uint32_t> ready;

        for (uint32_t passIdx = 0; passIdx < passCount; ++passIdx)
            if (remainingParents[passIdx] == 0)
                ready.push_back(passIdx);

        while (!ready.empty())
        {
            auto const passIdx = ready.front();
            ready.pop_front();

            result.push_back(passIdx);

            for (auto const childIdx : children[passIdx])
            {
                assert(remainingParents[childIdx] > 0);

                if (--remainingParents[childIdx] == 0)
                    ready.push_back(childIdx);
            }
        }

        assert(result.size() == passCount && "cycle detected in pass dependency graph");
        return result;
    }
}

namespace caldera
{
    void GraphIR::Factory::describe_execution(
        GraphIR& ir,
        std::vector<detail::PassDeclaration> const& passes,
        std::unordered_map<ImageID, std::vector<detail::ImageVersionDesc>> const& imageVersions,
        std::unordered_map<BufferID, std::vector<detail::BufferVersionDesc>> const& bufferVersions)
    {
        auto& execution = ir.m_impl->execution;

        std::vector<std::vector<uint32_t>> passChildren;
        std::vector<uint32_t> passParentCount;

        passChildren.resize(passes.size());
        passParentCount.resize(passes.size());

        ::append_tree(imageVersions, passParentCount, passChildren);
        ::append_tree(bufferVersions, passParentCount, passChildren);

        execution.passes = passes;
        execution.passOrder = ::topological_sort(passChildren, passParentCount);
    }

    detail::GraphExecutionIR const&
    GraphIR::ExecutionAttorney::get_execution(GraphIR const &ir) noexcept
    {
        return ir.m_impl->execution;
    }
}
