#include <cstdint>
#include <queue>
#include <vector>
#include <ranges>

#include <caldera/structs.hpp>
#include <caldera/detail/access_resolver.hpp>
#include <caldera/detail/graph_compiler.hpp>

#include "caldera/detail/touch_translator.hpp"

namespace
{
    [[nodiscard]] auto group_key(
        caldera::detail::ImageTouch const &touch,
        caldera::FamilyNumbers const &families) {
        return std::pair{ static_cast<uint32_t>(touch.layout), families.get(touch.family)};
    }

    [[nodiscard]] auto group_key(
        caldera::detail::BufferTouch const &touch,
        caldera::FamilyNumbers const &families) {
        return families.get(touch.family);
    }

    void sort_readings(
        auto const &writing,
        auto &readings,
        caldera::FamilyNumbers const &families) {
        auto const comparator = [&](auto const &left, auto const &right) {
            if (requires { left.layout; right.layout; })
                if (left.layout != right.layout)
                    return left.layout < right.layout;

            return families.get(left.family) < families.get(right.family);
        };

        std::vector<decltype(group_key(writing, families))> remaining;

        for (auto const &reading: readings)
            if (auto const key = group_key(reading, families);
                std::ranges::find(remaining, key) == remaining.end()) {
                remaining.push_back(key);
            }

        auto state = (writing.passIdx != UINT32_MAX)
                         ? group_key(writing, families)
                         : remaining.front();

        std::vector<decltype(group_key(writing, families))> order;
        order.reserve(remaining.size());

        while (!remaining.empty()) {
            auto const cost = [&](auto const &k) {
                if constexpr (requires{ state.first; })
                    return std::pair{ k.first != state.first, k.second != state.second };
                else
                    return k != state;
            };

            auto const best = std::ranges::min_element(remaining, {}, cost);

            state = *best;
            order.push_back(state);
            remaining.erase(best);
        }

        auto const rank = [&](auto const &reading) {
            return std::ranges::find(order, group_key(reading, families)) - order.begin();
        };

        std::ranges::stable_sort(readings, {}, rank);
    }

    void sort_versions(
        auto &versionTable,
        caldera::FamilyNumbers const &families)
    {
        for (auto &[readingID, versionList]: versionTable)
            for (auto &[writing, readingList]: versionList)
                sort_readings(writing, readingList, families);
    }

    template <class TableTy>
    void append_adjacency_list(
        caldera::detail::GraphAdjacencyList &adjacency,
        caldera::FamilyNumbers const &families,
        TableTy const &versionTable)
    {
        using TouchTy = decltype(std::declval<typename TableTy::mapped_type::value_type>().writing);

        std::vector<TouchTy> prevLayer;
        std::vector<TouchTy> currLayer;

        auto const bindWithPrevLayer = [&](auto const& touch)
        {
            for (auto const prev: prevLayer) {
                if (prev.passIdx == touch.passIdx)
                    continue;

                adjacency.passChildren[prev.passIdx].push_back(touch.passIdx);
                adjacency.passParents[touch.passIdx].push_back( caldera::PassDependency{
                    .otherPass = prev.passIdx,
                    .srcStages = prev.stages,
                    .dstStages = touch.stages
                } );
            }

            currLayer.push_back(touch);
        };

        auto const closeLayer = [&]
        {
            if (currLayer.empty())
                return;

            prevLayer.swap(currLayer);
            currLayer.clear();
        };

        for (auto const &[resourceID, versionList]: versionTable)
        {
            prevLayer.clear();
            currLayer.clear();

            for (size_t versionIdx = 0; versionIdx < versionList.size(); ++versionIdx)
            {
                auto const &version = versionList[versionIdx];

                if (version.writing.passIdx != UINT32_MAX)
                    bindWithPrevLayer(version.writing);

                closeLayer();

                auto const nextWriter = versionIdx + 1 < versionList.size()
                                      ? versionList[versionIdx + 1].writing.passIdx
                                      : UINT32_MAX;

                auto const &readers = version.readingList;
                decltype(::group_key(readers.front(), families)) lastKey{};

                for (auto const &reading: readers)
                {
                    if (reading.passIdx == nextWriter)
                        continue;

                    auto const key = ::group_key(reading, families);

                    if (key != lastKey)
                        closeLayer();

                    bindWithPrevLayer(reading);
                    lastKey = key;
                }

                closeLayer();
            }
        }
    }

    void deduplicate_adjacency_list(
        caldera::detail::GraphAdjacencyList &adjacency)
    {
        for (auto &children: adjacency.passChildren) {
            std::ranges::sort(children);
            children.erase(std::ranges::unique(children).begin(), children.end());
        }

        for (auto &parents : adjacency.passParents)
        {
            if (parents.empty())
                continue;

            std::ranges::sort(parents, {}, &caldera::PassDependency::otherPass);

            uint32_t writeIdx = 0;

            for (uint32_t readIdx = 1; readIdx < parents.size(); ++readIdx)
            {
                if (parents[writeIdx].otherPass == parents[readIdx].otherPass)
                {
                    parents[writeIdx].srcStages |= parents[readIdx].srcStages;
                    parents[writeIdx].dstStages |= parents[readIdx].dstStages;
                }
                else {
                    if (++writeIdx != readIdx)
                        parents[writeIdx] = parents[readIdx];
                }
            }

            parents.resize(writeIdx + 1);
        }

        assert(std::ranges::none_of(std::views::iota(0u, adjacency.passChildren.size()),
            [&] (uint32_t const i) { return std::ranges::binary_search(adjacency.passChildren[i], i); }));
    }

    [[nodiscard]] std::vector<uint32_t>
    topological_sort(
        caldera::detail::GraphAdjacencyList const &adjacency,
        caldera::FamilyNumbers const &families,
        std::span<caldera::detail::PassDeclaration const> const passes) {
        constexpr uint32_t slot_count = 3;
        constexpr uint32_t no_slot = slot_count;

        static_assert(static_cast<uint32_t>(caldera::FamilyType::graphics) == 0);
        static_assert(static_cast<uint32_t>(caldera::FamilyType::transfer) == 1);
        static_assert(static_cast<uint32_t>(caldera::FamilyType::compute) == 2);

        std::vector<uint32_t> result;
        std::vector<uint32_t> inDegree;
        std::array<std::queue<uint32_t>, 3> ready;

        auto const totalPassCount = adjacency.passParents.size();
        assert(totalPassCount == adjacency.passChildren.size() &&
            totalPassCount == passes.size());

        result.reserve(totalPassCount);
        inDegree.reserve(totalPassCount);

        std::array<uint32_t, slot_count> slotOfType{};

        for (uint32_t type = 0; type < slot_count; ++type) {
            slotOfType[type] = type;

            auto const number = families.get(static_cast<caldera::FamilyType>(type));

            for (uint32_t earlier = 0; earlier < type; ++earlier)
                if (families.get(static_cast<caldera::FamilyType>(earlier)) == number) {
                    slotOfType[type] = earlier;
                    break;
                }
        }

        auto const slotOf = [&](uint32_t const passIdx) {
            return slotOfType[static_cast<uint32_t>(passes[passIdx].family)];
        };

        for (auto const &parents: adjacency.passParents)
            inDegree.emplace_back(static_cast<uint32_t>(parents.size()));

        for (uint32_t passIdx = 0; passIdx < inDegree.size(); ++passIdx)
            if (inDegree[passIdx] == 0)
                ready[slotOf(passIdx)].push(passIdx);

        auto current = no_slot;

        auto const pickSlot = [&]() {
            if (current != no_slot && !ready[current].empty())
                return current;

            auto best = no_slot;

            for (uint32_t slotIdx = 0; slotIdx < slot_count; ++slotIdx)
                if (!ready[slotIdx].empty() &&
                    (best == no_slot || ready[slotIdx].front() < ready[best].front())) {
                    best = slotIdx;
                };

            return best;
        };

        for (;;) {
            current = pickSlot();

            if (current == no_slot)
                break;

            auto const passIdx = ready[current].front();
            ready[current].pop();

            result.emplace_back(passIdx);

            for (auto const childIdx: adjacency.passChildren[passIdx])
                if (--inDegree[childIdx] == 0)
                    ready[slotOf(childIdx)].emplace(childIdx);
        }

        assert(result.size() == totalPassCount && "found a cycle in the graph");
        return result;
    }

    /* * * * */

    template <class TableTy>
    [[nodiscard]] TableTy make_tracking_table(auto const& versionTable)
    {
        TableTy table;
        table.reserve(versionTable.size());

        caldera::detail::TouchTranslator translator;

        for (auto const& [resourceID, versionList] : versionTable)
        {
            auto& tracked = table[{}];

            auto const versionCount = static_cast<uint32_t>(versionList.size());
            bool firstTouchProcessed = false;

            for (uint32_t versionIdx = 0; versionIdx < versionCount; ++versionIdx)
            {
                auto const& version = versionList[versionIdx];

                if (!firstTouchProcessed)
                {
                    firstTouchProcessed = true;

                    if (version.writing.passIdx != UINT32_MAX)
                        tracked.firstTouch = version.writing;
                    else if (!version.readingList.empty())
                        tracked.firstTouch = version.readingList.front();
                    else
                        firstTouchProcessed = false;
                }

                if (versionIdx == versionCount - 1) {
                    if (!version.readingList.empty())
                        tracked.lastTouch = version.readingList.back();
                    else
                        tracked.lastTouch = version.writing;
                }

                tracked.usages |= translator.translate_to_usage_flags(version.writing);

                if constexpr (requires{ tracked.aspect; })
                    tracked.aspect |= translator.translate_to_aspect_mask(version.writing);

                for (auto const& reading : version.readingList)
                {
                    tracked.usages |= translator.translate_to_usage_flags(reading);

                    if constexpr (requires{ tracked.aspect; })
                        tracked.aspect |= translator.translate_to_aspect_mask(version.writing);
                }
            }
        }

        return table;
    }

    /* * * * */

    [[nodiscard]] caldera::detail::ImageBarrier
    make_initial_barrier(
        caldera::ImageID const id,
        caldera::FamilyNumbers const& families,
        caldera::detail::ImageTrackedInfo const& trackedInfo) noexcept
    {
        return caldera::detail::ImageBarrier {
            .srcStages = {},
            .srcAccess = {},
            .dstStages = trackedInfo.firstTouch.stages,
            .dstAccess = trackedInfo.firstTouch.access,
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = trackedInfo.firstTouch.layout,
            .srcFamilyIdx = vk::QueueFamilyIgnored,
            .dstFamilyIdx = families.get(trackedInfo.firstTouch.family),
            .id = id,
            .subresourceRange = {
                trackedInfo.aspect,
                0, vk::RemainingMipLevels,
                0, vk::RemainingArrayLayers
            }
        };
    }

    [[nodiscard]] caldera::detail::BufferBarrier
    make_initial_barrier(
        caldera::BufferID const id,
        caldera::FamilyNumbers const& families,
        caldera::detail::BufferTrackedInfo const& trackedInfo) noexcept
    {
        return caldera::detail::BufferBarrier {
            .srcStages = {},
            .srcAccess = {},
            .dstStages = trackedInfo.firstTouch.stages,
            .dstAccess = trackedInfo.firstTouch.access,
            .srcFamilyIdx = vk::QueueFamilyIgnored,
            .dstFamilyIdx = families.get(trackedInfo.firstTouch.family),
            .id = id,
            0, vk::WholeSize
        };
    }

    template<class TagTy>
    void append_with_barriers(
        caldera::detail::CompiledGraph &graph,
        caldera::FamilyNumbers const& families,
        auto const barrierGroupPtr,
        auto const preGroupOffsetPtr,
        auto const preGroupCountPtr,
        auto const postGroupOffsetPtr,
        auto const postGroupCountPtr,
        auto const &versionTable,
        auto const &trackingTable,
        auto const &passOrder)
    {
        constexpr auto undefined_touch = std::invoke([&] {
            if constexpr (std::same_as<TagTy, caldera::ImageTag>)
                return caldera::detail::undefined_image_touch;
            else
                return caldera::detail::undefined_buffer_touch;
        });

        using BarrierTy = std::conditional_t<std::same_as<TagTy, caldera::ImageTag>,
            caldera::detail::ImageBarrier, caldera::detail::BufferBarrier>;

        caldera::detail::AccessResolver resolver {};
        auto const passCount = passOrder.size();

        std::vector<std::vector<BarrierTy>> passPreBarriers;
        std::vector<std::vector<BarrierTy>> passPostBarriers;

        passPreBarriers.resize(passCount);
        passPostBarriers.resize(passCount);

        uint32_t totalPreBarrierCount = 0;
        uint32_t totalPostBarrierCount = 0;

        for (auto const& [resourceID, versionList] : versionTable)
        {
            auto const& trackedInfo = trackingTable.at(resourceID);

            auto srcTouch = undefined_touch;
            bool initVersion = true;

            if (resourceID.isTransient)
            {
                auto const& firstTouch = trackingTable.at(resourceID).firstTouch;
                auto const& lastTouch = trackingTable.at(resourceID).lastTouch;

                if (firstTouch.passIdx != UINT32_MAX && lastTouch.passIdx != UINT32_MAX)
                {
                    srcTouch.family = firstTouch.family;

                    if (families.get(firstTouch.family) == families.get(lastTouch.family))
                    {
                        srcTouch.stages = lastTouch.stages;
                        srcTouch.access = lastTouch.access;
                    }
                }
            }

            auto const processTransition = [&] (auto const& dstTouch)
            {
                switch (resolver.has_hazards(families, srcTouch, dstTouch))
                {
                case caldera::detail::HazardType::none: {
                    srcTouch.passIdx = dstTouch.passIdx;
                    srcTouch.stages |= dstTouch.stages;
                    srcTouch.access |= dstTouch.access;
                    break;
                }
                case caldera::detail::HazardType::access: {
                    auto const barrier = resolver.resolve_access_hazard(resourceID, srcTouch, dstTouch, trackedInfo);

                    passPreBarriers[dstTouch.passIdx].emplace_back(barrier);
                    ++totalPreBarrierCount;

                    srcTouch = dstTouch;
                    break;
                }
                case caldera::detail::HazardType::ownership: {
                    auto const [rlsBarrier, acqBarrier] =
                        resolver.resolve_ownership_hazard(resourceID, families, srcTouch, dstTouch, trackedInfo);

                    passPostBarriers[srcTouch.passIdx].emplace_back(rlsBarrier);
                    passPreBarriers[dstTouch.passIdx].emplace_back(acqBarrier);

                    ++totalPreBarrierCount;
                    ++totalPostBarrierCount;

                    srcTouch = dstTouch;
                    break;
                }
                default: assert(false);
                }
            };

            for (auto const& version: versionList)
            {
                if (!initVersion)
                    processTransition(version.writing);

                for (auto const& reading : version.readingList)
                    processTransition(reading);

                initVersion = false;
            }
        }

        auto& targetBarrierList = graph.*barrierGroupPtr;
        assert(targetBarrierList.empty());

        targetBarrierList.reserve(totalPreBarrierCount + totalPostBarrierCount);

        auto const pushBarriers = [&] (auto const& srcList)
        {
            auto const count = srcList.size();
            auto const offset = targetBarrierList.size();

            assert(targetBarrierList.capacity() >= offset + count);

            targetBarrierList.insert(targetBarrierList.end(), srcList.begin(), srcList.end());
            return std::make_pair(offset, count);
        };

        for (auto const& [resourceID, trackedInfo] : trackingTable)
            if (!resourceID.isTransient)
                passPreBarriers[trackedInfo.firstTouch.passIdx]
                    .emplace_back(::make_initial_barrier(resourceID, families, trackedInfo));

        for (auto const passIdx : passOrder)
        {
            auto& compiled = graph.compiledPasses[passIdx];

            auto const [preOffset, preCount] = pushBarriers(passPreBarriers[passIdx]);
            auto const [postOffset, postCount] = pushBarriers(passPostBarriers[passIdx]);

            compiled.*preGroupOffsetPtr = preOffset;
            compiled.*preGroupCountPtr = preCount;
            compiled.*postGroupOffsetPtr = postOffset;
            compiled.*postGroupCountPtr = postCount;
        }
    }

    /* * * * */

    template <class TagTy>
    void append_with_allowed_resources(
        caldera::detail::CompiledGraph &graph,
        auto const resourceGroupPtr,
        auto const resourceOffsetPtr,
        auto const resourceCountPtr,
        auto const& versionTable,
        auto const& passOrder)
    {
        auto const passCount = passOrder.size();

        std::vector<std::vector<caldera::BasicResourceID<TagTy>>> passAllowedResources;
        passAllowedResources.resize(passCount);

        for (auto const& [resourceID, versionList] : versionTable)
            for (auto const& version : versionList)
            {
                if (version.writing.passIdx != UINT32_MAX)
                    passAllowedResources[version.writing.passIdx].emplace_back(resourceID);

                for (auto const& reading : version.readingList)
                    passAllowedResources[reading.passIdx].emplace_back(resourceID);
            }

        for (auto& resourceList : passAllowedResources) {
            std::ranges::sort(resourceList);
            resourceList.erase(std::ranges::unique(resourceList).begin(), resourceList.end());
        };

        auto& targetResourceList = graph.*resourceGroupPtr;
        assert(targetResourceList.empty());

        targetResourceList.reserve(versionTable.size());

        for (auto const passIdx : passOrder)
        {
            auto& compiled = graph.compiledPasses[passIdx];
            auto& allowedResources = passAllowedResources[passIdx];

            auto const count = allowedResources.size();
            auto const offset = targetResourceList.size();

            assert(targetResourceList.capacity() >= offset + count);

            targetResourceList.insert(targetResourceList.end(), allowedResources.begin(), allowedResources.end());
            compiled.*resourceOffsetPtr = offset;
            compiled.*resourceCountPtr = count;
        }
    }

    /* * * * */

    void append_with_pass_dependencies(
        caldera::detail::CompiledGraph &graph,
        caldera::detail::GraphAdjacencyList const& adjacency,
        auto const& passOrder)
    {
        auto const passCount = passOrder.size();

        std::vector<uint32_t> passExecutionIndex(passCount);

        for (uint32_t execIdx = 0; execIdx < passCount; ++execIdx)
            passExecutionIndex[passOrder[execIdx]] = execIdx;

        uint32_t totalDeps = 0;

        for (auto const &parents : adjacency.passParents)
            totalDeps += parents.size();

        assert(graph.passDependencies.empty());
        graph.passDependencies.reserve(totalDeps);

        for (auto const passIdx : passOrder)
        {
            auto const &parents = adjacency.passParents[passIdx];
            auto const offset = graph.passDependencies.size();
            auto const count = parents.size();

            for (auto const& dep : parents)
                graph.passDependencies.emplace_back(passExecutionIndex[dep.otherPass], dep.srcStages, dep.dstStages);

            graph.compiledPasses[passIdx].passDependencyOffset = offset;
            graph.compiledPasses[passIdx].passDependencySize = count;
        }
    }

    /* * * * */

    [[nodiscard]] std::vector<caldera::detail::CompiledPass>
    make_passes(
        caldera::GraphIR const& ir,
        caldera::FamilyNumbers const& families)
    {
        auto const passCount = ir.passes.size();

        std::vector<caldera::detail::CompiledPass> passes;
        passes.reserve(passCount);

        for (auto const& passInfo : ir.passes)
            passes.emplace_back(
                passInfo.name,
                families.get(passInfo.family),
                passInfo.callback);

        return passes;
    }

    [[nodiscard]] caldera::detail::CompiledGraph
    generate_compiled_graph(
        caldera::GraphIR const& ir,
        caldera::FamilyNumbers const& families,
        caldera::detail::GraphAdjacencyList const& adjacency,
        std::vector<uint32_t> const &passOrder)
    {
        caldera::detail::CompiledGraph result;
        result.compiledPasses = ::make_passes(ir, families);

        result.trackedImages = ::make_tracking_table<caldera::detail::ImageTrackingTable>(ir.imageVersions);
        result.trackedBuffer = ::make_tracking_table<caldera::detail::BufferTrackingTable>(ir.bufferVersions);

        ::append_with_barriers<caldera::ImageTag>(
            result,
            families,
            &caldera::detail::CompiledGraph::imageBarriers,
            &caldera::detail::CompiledPass::imagePreBarrierOffset,
            &caldera::detail::CompiledPass::imagePreBarrierSize,
            &caldera::detail::CompiledPass::imagePostBarrierOffset,
            &caldera::detail::CompiledPass::imagePostBarrierSize,
            ir.imageVersions,
            result.trackedImages,
            passOrder);

        ::append_with_barriers<caldera::BufferTag>(
            result,
            families,
            &caldera::detail::CompiledGraph::bufferBarriers,
            &caldera::detail::CompiledPass::bufferPreBarrierOffset,
            &caldera::detail::CompiledPass::bufferPreBarrierSize,
            &caldera::detail::CompiledPass::bufferPostBarrierOffset,
            &caldera::detail::CompiledPass::bufferPostBarrierSize,
            ir.bufferVersions,
            result.trackedBuffer,
            passOrder);

        ::append_with_allowed_resources<caldera::ImageTag>(
            result,
            &caldera::detail::CompiledGraph::allowedImages,
            &caldera::detail::CompiledPass::allowedImageOffset,
            &caldera::detail::CompiledPass::allowedImageSize,
            ir.imageVersions,
            passOrder);

        ::append_with_allowed_resources<caldera::BufferTag>(
            result,
            &caldera::detail::CompiledGraph::allowedBuffers,
            &caldera::detail::CompiledPass::allowedBufferOffset,
            &caldera::detail::CompiledPass::allowedBufferSize,
            ir.bufferVersions,
            passOrder);

        ::append_with_pass_dependencies(result, adjacency, passOrder);

        auto passes = std::move(result.compiledPasses);
        result.compiledPasses.reserve(passOrder.size());

        for (auto const passIdx : passOrder)
            result.compiledPasses.emplace_back(std::move(passes[passIdx]));

        return result;
    }
}

namespace caldera::detail {
    CompiledGraph GraphCompiler::compile(
        FamilyNumbers const &families,
        GraphIR ir)
    {
        ::sort_versions(ir.imageVersions, families);
        ::sort_versions(ir.bufferVersions, families);

        GraphAdjacencyList adjacency;
        adjacency.passParents.resize(ir.passes.size());
        adjacency.passChildren.resize(ir.passes.size());

        ::append_adjacency_list(adjacency, families, ir.imageVersions);
        ::append_adjacency_list(adjacency, families, ir.bufferVersions);
        ::deduplicate_adjacency_list(adjacency);

        auto const order = ::topological_sort(adjacency, families, ir.passes);
        return ::generate_compiled_graph(ir, families, adjacency, order);
    }
}