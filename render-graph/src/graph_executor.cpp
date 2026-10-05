#include <caldera/graph_executor.h>
#include <caldera/structs.h>

#include "caldera/command_buffer_distributor.h"
#include "caldera/graph_allocator.h"
#include "caldera/resource_map.h"
#include "caldera/resource_state_manager.h"
#include "caldera/semaphore_map.h"
#include "detail/access_resolver.h"
#include "detail/command_buffer_distributor.h"
#include "detail/render_graph.h"
#include "detail/resource_map.h"
#include "detail/resource_state_manager.h"
#include "detail/semaphore_map.h"
#include "detail/utility.h"

namespace
{
    /* Barriers */

    struct TranslatedPassBarriers
    {
        std::vector<vk::ImageMemoryBarrier2> imagePreBarriers;
        std::vector<vk::BufferMemoryBarrier2> bufferPreBarriers;

        std::vector<vk::ImageMemoryBarrier2> imagePostBarriers;
        std::vector<vk::BufferMemoryBarrier2> bufferPostBarriers;
    };

    [[nodiscard]] vk::ImageMemoryBarrier2
    translate_one_barrier(
        caldera::FamilyNumbers const& families,
        caldera::ResourceMap const& resourceMap,
        caldera::detail::ImageBarrier const& barrier,
        auto const trackedInfosPtr)
    {
        return vk::ImageMemoryBarrier2
        {
            barrier.srcStage,
            barrier.srcAccess,
            barrier.dstStage,
            barrier.dstAccess,
            barrier.oldLayout,
            barrier.newLayout,
            families.get(barrier.srcFamily),
            families.get(barrier.dstFamily),
            resourceMap.at(barrier.image),
            { trackedInfosPtr->at(barrier.image).aspects, 0, vk::RemainingMipLevels, 0, vk::RemainingArrayLayers }
        };
    }

    [[nodiscard]] vk::BufferMemoryBarrier2
    translate_one_barrier(
        caldera::FamilyNumbers const& families,
        caldera::ResourceMap const& resourceMap,
        caldera::detail::BufferBarrier const& barrier,
        [[maybe_unused]] void const* = nullptr)
    {
        return vk::BufferMemoryBarrier2
        {
            barrier.srcStage,
            barrier.srcAccess,
            barrier.dstStage,
            barrier.dstAccess,
            families.get(barrier.srcFamily),
            families.get(barrier.dstFamily),
            resourceMap.at(barrier.buffer),
            0, vk::WholeSize
        };
    }

    [[nodiscard]] std::vector<TranslatedPassBarriers>
    translate_barriers(
        auto const& passBarriers,
        auto const& imageTrackedInfos,
        caldera::FamilyNumbers const& families,
        caldera::ResourceMap const& resourceMap)
    {
        std::vector<TranslatedPassBarriers> result;
        result.reserve(passBarriers.size());

        auto const processGroup = [&] (auto const& in, auto& out)
        {
            out.reserve(in.size());

            for (auto const& inBarrier : in)
                out.push_back(::translate_one_barrier(families, resourceMap, inBarrier, &imageTrackedInfos));
        };

        for (auto const& input : passBarriers)
        {
            auto& output = result.emplace_back();

            processGroup(input.imagePreBarriers, output.imagePreBarriers);
            processGroup(input.bufferPreBarriers, output.bufferPreBarriers);

            processGroup(input.imagePostBarriers, output.imagePostBarriers);
            processGroup(input.bufferPostBarriers, output.bufferPostBarriers);
        }

        return result;
    }

    void deploy_initial_barriers(
        caldera::FamilyNumbers const& families,
        caldera::ResourceStateManager const& stateManager,
        caldera::ResourceMap const& resourceMap,
        auto& barriers,
        auto const barrierGroupPtr,
        auto const& trackedInfos)
    {
        for (auto const& [resourceID, info] : trackedInfos)
        {
            auto const handle = resourceMap.at(resourceID);

            auto& group = barriers[info.firstPass].*barrierGroupPtr;
            auto const& lastTouch = caldera::ResourceStateManager::ExecutorAttorney::get_state(stateManager, resourceID);

            caldera::detail::AccessResolver resolver;
            auto const hazard = resolver.has_hazards(lastTouch, info.firstTouch);

            if (hazard == caldera::detail::HazardType::none)
                continue;

            auto const barrier = hazard == caldera::detail::HazardType::access
                ? resolver.resolve_access_hazard(resourceID, lastTouch, info.firstTouch)
                : resolver.resolve_ownership_hazard(resourceID, lastTouch, info.firstTouch)[0];

            auto const translated = ::translate_one_barrier(families, resourceMap, barrier, &trackedInfos);
            group.push_back(translated);
        }
    }

    void apply_barriers(
        vk::CommandBuffer const cmd,
        auto const& imageGroup,
        auto const& bufferGroup)
    {
        vk::DependencyInfo info;
        info.setImageMemoryBarriers(imageGroup);
        info.setBufferMemoryBarriers(bufferGroup);

        cmd.pipelineBarrier2(info);
    }

    /* Semaphores */

    struct TranslatedPassSemaphores
    {
        std::vector<vk::SemaphoreSubmitInfo> waitSemaphores;
        vk::SemaphoreSubmitInfo signalSemaphores;
    };

    [[nodiscard]] std::vector<TranslatedPassSemaphores>
    translate_semaphores(
        auto const& passInfos,
        auto const& passBarriers,
        caldera::SemaphoreMap& semaphoreMap)
    {
        std::vector<TranslatedPassSemaphores> result;
        result.resize(passBarriers.size());

        for (uint32_t passIdx = 0; passIdx < passInfos.size(); ++passIdx)
            for (auto const& [otherPassIdx, stages] : passBarriers[passIdx].waitDeps)
            {
                auto const family = passInfos[otherPassIdx].family;
                auto const semaphore = caldera::SemaphoreMap
                    ::ExecutorAttorney::get_semaphore(semaphoreMap, family);

                auto& signal = result[otherPassIdx].signalSemaphores;

                assert(signal.semaphore == VK_NULL_HANDLE ||
                      (signal.semaphore == semaphore.handle && signal.value == semaphore.timeline + otherPassIdx));

                signal.semaphore = semaphore.handle;
                signal.value = semaphore.timeline + otherPassIdx;
                signal.stageMask |= stages;

                result[passIdx].waitSemaphores.emplace_back(
                   semaphore.handle, semaphore.timeline + otherPassIdx, stages);
            }

        return result;
    }

    /* * * * */

    void process_cmd(
        caldera::ResourceMap const& map,
        vk::CommandBuffer const cmd,
        TranslatedPassBarriers const& barriers,
        auto const& callback)
    {
        cmd.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
        //caldera::detail::assert_result(result, "failed to begin a cmd");

        ::apply_barriers(cmd, barriers.imagePreBarriers, barriers.bufferPreBarriers);

        std::invoke(callback, map, cmd);

        ::apply_barriers(cmd, barriers.imagePostBarriers, barriers.bufferPostBarriers);
        cmd.end();
    }

    void dump_touches(
        caldera::ResourceStateManager& stateManager,
        auto const& trackedInfos)
    {
        for (auto const& [resourceID, info] : trackedInfos)
            if (!resourceID.is_transient)
            {
                auto& state = caldera::ResourceStateManager
                    ::ExecutorAttorney::get_state(stateManager, resourceID);

                state = info.lastTouch;
            }
    }
}

namespace caldera
{
    SubmittableGraph GraphExecutor::execute(
        RenderGraph const &graph,
        ExecutionContext const &context)
    {
        auto const& compiled = RenderGraph::ExecutorAttorney::get_compiled(graph);
        auto const& allocated = RenderGraph::ExecutorAttorney::get_allocated(graph);

        ResourceMap::TransientAttorney::import_graph(*context.resourceMap, allocated);
        ResourceStateManager::ExecutorAttorney::cleanup_transient(*context.stateManager);

        auto translatedBarriers = ::translate_barriers(
            compiled.barriers, compiled.imageTrackedInfos, context.families, *context.resourceMap);

        ::deploy_initial_barriers(context.families, *context.stateManager, *context.resourceMap,
            translatedBarriers, &::TranslatedPassBarriers::imagePreBarriers, compiled.imageTrackedInfos);

        ::deploy_initial_barriers(context.families, *context.stateManager, *context.resourceMap,
            translatedBarriers, &::TranslatedPassBarriers::bufferPreBarriers, compiled.bufferTrackedInfos);

        auto const translatedSemaphores = ::translate_semaphores(
            compiled.passes, compiled.barriers, *context.semaphoreMap);

        SubmittableGraph result;
        result.passes.reserve(compiled.passes.size());

        for (uint32_t passIdx = 0; passIdx < compiled.passes.size(); ++passIdx)
        {
            auto const& passInfo = compiled.passes[passIdx];
            auto const& barriers = translatedBarriers[passIdx];
            auto const& semaphores = translatedSemaphores[passIdx];

            auto const cmd = CommandBufferDistributor
                ::ConsumerAttorney::take_buffer(*context.distributor, passInfo.family);

            ::process_cmd(*context.resourceMap, cmd, barriers, passInfo.callback);

            result.passes.emplace_back(
                passInfo.family, cmd,
                semaphores.signalSemaphores.semaphore == VK_NULL_HANDLE
                ? std::vector<vk::SemaphoreSubmitInfo>{} : std::vector{ semaphores.signalSemaphores },
                semaphores.waitSemaphores);
        }

        ::dump_touches(*context.stateManager, compiled.imageTrackedInfos);
        ::dump_touches(*context.stateManager, compiled.bufferTrackedInfos);

        ResourceMap::TransientAttorney::reset_transient(*context.resourceMap);

        return result;
    }
}
