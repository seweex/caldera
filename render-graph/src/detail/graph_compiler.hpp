#pragma once

#include <cassert>
#include <cstdint>
#include <queue>
#include <vector>
#include <ranges>

#include <caldera/structs.hpp>
#include <caldera/detail/access_resolver.hpp>
#include <caldera/detail/touch_translator.hpp>
#include <caldera/detail/graph_compiler.hpp>

namespace caldera::detail
{
    [[nodiscard]] inline auto group_key(
         ImageTouch const &touch,
         FamilyNumbers const &families) {
        return std::pair{ static_cast<uint32_t>(touch.layout), families.get(touch.family)};
    }

    [[nodiscard]] inline auto group_key(
         BufferTouch const &touch,
         FamilyNumbers const &families) {
        return families.get(touch.family);
    }

    inline void sort_readings(
        auto const &writing,
        auto &readings,
         FamilyNumbers const &families)
    {
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

    inline void sort_versions(
        auto &versionTable,
         FamilyNumbers const &families)
    {
        for (auto &[readingID, versionList]: versionTable)
            for (auto &[writing, readingList]: versionList)
                sort_readings(writing, readingList, families);
    }

    template <class TableTy>
    inline void append_adjacency_list(
         GraphAdjacencyList &adjacency,
         FamilyNumbers const &families,
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
                adjacency.passParents[touch.passIdx].push_back(  PassDependency{
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
                decltype(group_key(readers.front(), families)) lastKey{};

                for (auto const &reading: readers)
                {
                    if (reading.passIdx == nextWriter)
                        continue;

                    auto const key = group_key(reading, families);

                    if (key != lastKey)
                        closeLayer();

                    bindWithPrevLayer(reading);
                    lastKey = key;
                }

                closeLayer();
            }
        }
    }

    inline void deduplicate_adjacency_list(
         GraphAdjacencyList &adjacency)
    {
        for (auto &children: adjacency.passChildren) {
            std::ranges::sort(children);
            children.erase(std::ranges::unique(children).begin(), children.end());
        }

        for (auto &parents : adjacency.passParents)
        {
            if (parents.empty())
                continue;

            std::ranges::sort(parents, {}, & PassDependency::otherPass);

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
    inline topological_sort(
         GraphAdjacencyList const &adjacency,
         FamilyNumbers const &families,
        std::span< PassDeclaration const> const passes) {
        constexpr uint32_t slot_count = 3;
        constexpr uint32_t no_slot = slot_count;

        static_assert(static_cast<uint32_t>( FamilyType::graphics) == 0);
        static_assert(static_cast<uint32_t>( FamilyType::transfer) == 1);
        static_assert(static_cast<uint32_t>( FamilyType::compute) == 2);

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

            auto const number = families.get(static_cast< FamilyType>(type));

            for (uint32_t earlier = 0; earlier < type; ++earlier)
                if (families.get(static_cast< FamilyType>(earlier)) == number) {
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
    [[nodiscard]] inline TableTy make_tracking_table(auto const& versionTable)
    {
        TableTy table;
        table.reserve(versionTable.size());

         TouchTranslator translator;

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

    [[nodiscard]]  ImageBarrier
    inline make_initial_barrier(
         ImageID const id,
         FamilyNumbers const& families,
         ImageTrackedInfo const& trackedInfo) noexcept
    {
        return  ImageBarrier {
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

    [[nodiscard]]  BufferBarrier
    inline make_initial_barrier(
         BufferID const id,
         FamilyNumbers const& families,
         BufferTrackedInfo const& trackedInfo) noexcept
    {
        return  BufferBarrier {
            .srcStages = {},
            .srcAccess = {},
            .dstStages = trackedInfo.firstTouch.stages,
            .dstAccess = trackedInfo.firstTouch.access,
            .srcFamilyIdx = vk::QueueFamilyIgnored,
            .dstFamilyIdx = families.get(trackedInfo.firstTouch.family),
            .id = id,
            .offset = 0,
            .size = vk::WholeSize
        };
    }

    template<class TagTy>
    inline void append_with_barriers(
         CompiledGraph &graph,
         FamilyNumbers const& families,
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
            if constexpr (std::same_as<TagTy,  ImageTag>)
                return  undefined_image_touch;
            else
                return  undefined_buffer_touch;
        });

        using BarrierTy = std::conditional_t<std::same_as<TagTy,  ImageTag>,
             ImageBarrier,  BufferBarrier>;

         AccessResolver resolver {};
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
                case  HazardType::none: {
                    srcTouch.passIdx = dstTouch.passIdx;
                    srcTouch.stages |= dstTouch.stages;
                    srcTouch.access |= dstTouch.access;
                    break;
                }
                case  HazardType::access: {
                    auto const barrier = resolver.resolve_access_hazard(resourceID, srcTouch, dstTouch, trackedInfo);

                    passPreBarriers[dstTouch.passIdx].emplace_back(barrier);
                    ++totalPreBarrierCount;

                    srcTouch = dstTouch;
                    break;
                }
                case  HazardType::ownership: {
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
                    .emplace_back(make_initial_barrier(resourceID, families, trackedInfo));

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
    inline void append_with_allowed_resources(
         CompiledGraph &graph,
        auto const resourceGroupPtr,
        auto const resourceOffsetPtr,
        auto const resourceCountPtr,
        auto const& versionTable,
        auto const& passOrder)
    {
        auto const passCount = passOrder.size();

        std::vector<std::vector< BasicResourceID<TagTy>>> passAllowedResources;
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

            targetResourceList.insert(targetResourceList.end(), allowedResources.begin(), allowedResources.end());
            compiled.*resourceOffsetPtr = offset;
            compiled.*resourceCountPtr = count;
        }
    }

    /* * * * */

    inline void append_with_pass_dependencies(
         CompiledGraph &graph,
         GraphAdjacencyList const& adjacency,
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

    [[nodiscard]] std::vector< CompiledPass>
    inline make_passes(
         GraphIR const& ir,
         FamilyNumbers const& families)
    {
        auto const passCount = ir.passes.size();

        std::vector< CompiledPass> passes;
        passes.reserve(passCount);

        for (auto const& passInfo : ir.passes)
            passes.emplace_back(
                passInfo.name,
                families.get(passInfo.family),
                passInfo.callback);

        return passes;
    }

    [[nodiscard]]  CompiledGraph
    inline generate_compiled_graph(
         GraphIR const& ir,
         FamilyNumbers const& families,
         GraphAdjacencyList const& adjacency,
        std::vector<uint32_t> const &passOrder)
    {
         CompiledGraph result;
        result.compiledPasses = make_passes(ir, families);

        result.trackedImages = make_tracking_table<ImageTrackingTable>(ir.imageVersions);
        result.trackedBuffers = make_tracking_table<BufferTrackingTable>(ir.bufferVersions);

        append_with_barriers< ImageTag>(
            result,
            families,
            & CompiledGraph::imageBarriers,
            & CompiledPass::imagePreBarrierOffset,
            & CompiledPass::imagePreBarrierSize,
            & CompiledPass::imagePostBarrierOffset,
            & CompiledPass::imagePostBarrierSize,
            ir.imageVersions,
            result.trackedImages,
            passOrder);

        append_with_barriers< BufferTag>(
            result,
            families,
            & CompiledGraph::bufferBarriers,
            & CompiledPass::bufferPreBarrierOffset,
            & CompiledPass::bufferPreBarrierSize,
            & CompiledPass::bufferPostBarrierOffset,
            & CompiledPass::bufferPostBarrierSize,
            ir.bufferVersions,
            result.trackedBuffers,
            passOrder);

        append_with_allowed_resources< ImageTag>(
            result,
            & CompiledGraph::allowedImages,
            & CompiledPass::allowedImageOffset,
            & CompiledPass::allowedImageSize,
            ir.imageVersions,
            passOrder);

        append_with_allowed_resources< BufferTag>(
            result,
            & CompiledGraph::allowedBuffers,
            & CompiledPass::allowedBufferOffset,
            & CompiledPass::allowedBufferSize,
            ir.bufferVersions,
            passOrder);

        append_with_pass_dependencies(result, adjacency, passOrder);

        auto passes = std::move(result.compiledPasses);
        result.compiledPasses.reserve(passOrder.size());

        for (auto const passIdx : passOrder)
            result.compiledPasses.emplace_back(std::move(passes[passIdx]));

        return result;
    }
}