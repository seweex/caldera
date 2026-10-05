#include <ranges>

#include <caldera/render_graph.h>
#include <caldera/graph_ir.h>
#include <caldera/graph_compiler.h>

#include "detail/render_graph.h"
#include "detail/graph_ir.h"

#include "detail/structs.h"
#include "detail/access_resolver.h"
#include "detail/usage_translator.h"

namespace
{
    void push_deps(
        auto& passBarriers,
        uint32_t const srcPassIdx,
        uint32_t const dstPassIdx,
        vk::PipelineStageFlags2 const srcFlags,
        vk::PipelineStageFlags2 const dstFlags)
    {
        auto const pushDep = [&] (
            auto& list,
            auto const index,
            auto const flags)
        {
            if (auto const iter = std::ranges::find(list, index, &caldera::PassDependency::otherPassIdx);
                iter != list.end())
            {
                iter->stages |= flags;
            }
            else
                list.emplace_back(index, flags);
        };

        pushDep(passBarriers[srcPassIdx].signalDeps, dstPassIdx, srcFlags);
        pushDep(passBarriers[dstPassIdx].waitDeps, srcPassIdx, dstFlags);
    }

    template <class TagTy>
    void fill(
        caldera::detail::CompiledGraph& compiled,
        auto const preBarriersPtr,
        auto const postBarriersPtr,
        auto const& versionTable,
        [[maybe_unused]] auto const& transientTrackedInfos)
    {
        caldera::detail::AccessResolver resolver = {};
        caldera::detail::UsageTranslator translator = {};

        for (auto const& [resourceID, versionList] : versionTable)
        {
            uint32_t lastToucher;
            caldera::detail::ResourceTouch lastTouch;

            if (!versionList[0].readings.empty())
            {
                auto const& touch = versionList[0].readings[0];

                lastToucher = 0;
                lastTouch = translator.translate_usage(touch.family, touch.usage, caldera::detail::AccessKind::read);
            }
            else {
                auto const& touch = versionList[1].writing;

                lastToucher = 1;
                lastTouch = translator.translate_usage(touch.family, touch.usage, caldera::detail::AccessKind::write);
            }

            auto const revealAndPushBarriers = [&] (
                auto const& toucher, caldera::detail::AccessKind const kind)
            {
                switch (auto const translatedTouch = translator.translate_usage(toucher.family, toucher.usage, kind);
                        resolver.has_hazards(lastTouch, translatedTouch))
                {
                    case caldera::detail::HazardType::none: {
                        lastTouch.access |= translatedTouch.access;
                        lastTouch.stages |= translatedTouch.stages;
                        break;
                    }
                    case caldera::detail::HazardType::access: {
                        auto const barrier = resolver.resolve_access_hazard(
                            resourceID, lastTouch, translatedTouch);

                        (compiled.barriers[toucher.passIdx].*preBarriersPtr).push_back(barrier);
                        lastTouch = translatedTouch;
                        break;
                    }
                    case caldera::detail::HazardType::ownership: {
                        auto const [rlsBarrier, acqBarrier] = resolver.resolve_ownership_hazard(
                            resourceID, lastTouch, translatedTouch);

                        (compiled.barriers[lastToucher].*postBarriersPtr).push_back(rlsBarrier);
                        (compiled.barriers[toucher.passIdx].*preBarriersPtr).push_back(acqBarrier);

                        ::push_deps(compiled.barriers, lastToucher,
                            toucher.passIdx, rlsBarrier.srcStage, acqBarrier.dstStage);

                        lastTouch = translatedTouch;
                        break;
                    }
                    default: assert(false);
                }

                lastToucher = toucher.passIdx;
            };

            bool firstVersion = true;

            for (auto const& [writing, readings] : versionList | std::views::drop(1))
            {
                if (!firstVersion || lastToucher != 1)
                    revealAndPushBarriers(writing, caldera::detail::AccessKind::write);

                if (!readings.empty())
                {
                    assert(std::ranges::is_sorted(readings,
                        typename decltype(readings)::value_type::Comparator{}));

                    for (auto const& reading : readings)
                        revealAndPushBarriers(reading, caldera::detail::AccessKind::read);
                }

                firstVersion = false;
            }
        }
    }
}

namespace caldera
{
    void GraphCompiler::compile(
        GraphIR const& ir,
        RenderGraph& graph)
    {
        detail::CompiledGraph compiled;

        auto const& execution = GraphIR::ExecutionAttorney::get_execution(ir);
        auto const& resources = GraphIR::ResourcesAttorney::get_resources(ir);

        compiled.passes = execution.passes;
        compiled.barriers.resize(execution.passOrder.size());

        compiled.imageTrackedInfos = resources.imageTrackedInfos;
        compiled.bufferTrackedInfos = resources.bufferTrackedInfos;

        ::fill<ImageTag>(compiled,
            &detail::PassBarriers::imagePreBarriers,
            &detail::PassBarriers::imagePostBarriers,
            resources.imageVersions,
            resources.imageTrackedInfos);

        ::fill<BufferTag>(compiled,
            &detail::PassBarriers::bufferPreBarriers,
            &detail::PassBarriers::bufferPostBarriers,
            resources.bufferVersions,
            resources.bufferTrackedInfos);

        RenderGraph::CompilerAttorney::push_compiled(graph, std::move(compiled));
    }
}
