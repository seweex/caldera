#include <expected>
#include <vulkan/vulkan.hpp>

#include <caldera/structs.hpp>
#include <caldera/render_graph.hpp>
#include <caldera/resource_map.hpp>
#include <caldera/command_buffer_distributor.hpp>
#include <caldera/resource_state_manager.hpp>

#include <caldera/graph_executor.hpp>

namespace
{
    [[nodiscard]] vk::ImageMemoryBarrier2
    translate_one_barrier(
        caldera::detail::ImageBarrier const& barrier,
        caldera::ResourceMap const& resourceMap,
        caldera::ResourceStateManager const& stateManager)
    {
        auto const handle = resourceMap.at(barrier.id);

        if (barrier.srcFamilyIdx == vk::QueueFamilyIgnored &&
            barrier.dstFamilyIdx != vk::QueueFamilyIgnored)
        {
            auto const lastState = stateManager.get_last_state(barrier.id);

            if (lastState.familyIdx != barrier.dstFamilyIdx)
                return vk::ImageMemoryBarrier2 {
                    {},
                    {},
                    barrier.dstStages,
                    barrier.dstAccess,
                    barrier.oldLayout,
                    barrier.newLayout,
                    barrier.srcFamilyIdx,
                    barrier.dstFamilyIdx,
                    handle,
                    barrier.subresourceRange
                };
            else
                return vk::ImageMemoryBarrier2 {
                    barrier.srcStages,
                    barrier.srcAccess,
                    barrier.dstStages,
                    barrier.dstAccess,
                    barrier.oldLayout,
                    barrier.newLayout,
                    vk::QueueFamilyIgnored,
                    vk::QueueFamilyIgnored,
                    handle,
                    barrier.subresourceRange
                };
        }
        else return vk::ImageMemoryBarrier2 {
            barrier.srcStages,
            barrier.srcAccess,
            barrier.dstStages,
            barrier.dstAccess,
            barrier.oldLayout,
            barrier.newLayout,
            barrier.srcFamilyIdx,
            barrier.dstFamilyIdx,
            handle,
            barrier.subresourceRange
        };
    }

    [[nodiscard]] vk::BufferMemoryBarrier2
    translate_one_barrier(
        caldera::detail::BufferBarrier const& barrier,
        caldera::ResourceMap const& resourceMap,
        caldera::ResourceStateManager const& stateManager)
    {
        auto const handle = resourceMap.at(barrier.id);

        if (barrier.srcFamilyIdx == vk::QueueFamilyIgnored &&
            barrier.dstFamilyIdx != vk::QueueFamilyIgnored)
        {
            auto const lastState = stateManager.get_last_state(barrier.id);

            if (lastState.familyIdx != barrier.dstFamilyIdx)
                return vk::BufferMemoryBarrier2 {
                    {},
                    {},
                    barrier.dstStages,
                    barrier.dstAccess,
                    barrier.srcFamilyIdx,
                    barrier.dstFamilyIdx,
                    handle,
                    barrier.offset,
                    barrier.size
                };
            else
                return vk::BufferMemoryBarrier2 {
                    barrier.srcStages,
                    barrier.srcAccess,
                    barrier.dstStages,
                    barrier.dstAccess,
                    vk::QueueFamilyIgnored,
                    vk::QueueFamilyIgnored,
                    handle,
                    barrier.offset,
                    barrier.size
                };
        }
        else return vk::BufferMemoryBarrier2 {
            barrier.srcStages,
            barrier.srcAccess,
            barrier.dstStages,
            barrier.dstAccess,
            barrier.srcFamilyIdx,
            barrier.dstFamilyIdx,
            handle,
            barrier.offset,
            barrier.size
        };
    }
}

namespace
{
    void prepare_execution_memory(
        caldera::ExecutionMemory& memory,
        caldera::detail::CompiledGraph const& graph)
    {
        memory.imageBarriers.clear();
        memory.bufferBarriers.clear();

        memory.passDependencies.clear();
        memory.submittablePasses.clear();

        memory.imageBarriers.reserve(graph.imageBarriers.size() + graph.trackedImages.size());
        memory.bufferBarriers.reserve(graph.bufferBarriers.size() + graph.trackedBuffer.size());

        memory.passDependencies.reserve(graph.passDependencies.size());
        memory.submittablePasses.reserve(graph.compiledPasses.size());
    }

    void translate_compiled_barriers(
        caldera::ExecutionMemory& memory,
        caldera::detail::CompiledGraph const& graph,
        caldera::ResourceMap const& resourceMap,
        caldera::ResourceStateManager const& stateManager)
    {
        memory.imageBarriers.resize(graph.imageBarriers.size());
        memory.bufferBarriers.resize(graph.bufferBarriers.size());

        auto const func = [&] (auto const& barrier)
            { return ::translate_one_barrier(barrier, resourceMap, stateManager); };

        std::ranges::transform(graph.imageBarriers, memory.imageBarriers.begin(), func);
        std::ranges::transform(graph.bufferBarriers, memory.bufferBarriers.begin(), func);
    }

    void copy_pass_dependencies(
        caldera::ExecutionMemory& memory,
        caldera::detail::CompiledGraph const& graph)
    {
        memory.passDependencies.assign(
            graph.passDependencies.begin(), graph.passDependencies.end());
    }

    [[nodiscard]] vk::Result process_cmd(
        vk::CommandBuffer const cmd,
        caldera::ExecutionMemory const& memory,
        caldera::ResourceMap const& resourceMap,
        caldera::detail::CompiledPass const& pass)
    {
        auto const applyBarriers = [&] (
            auto const& imageBarrierList,
            auto const& bufferBarrierList,
            uint32_t const imageOffset,
            uint32_t const imageSize,
            uint32_t const bufferOffset,
            uint32_t const bufferSize)
        {
            vk::DependencyInfo const dependencyInfo {
                {},
                0, nullptr,
                bufferSize, bufferBarrierList.data() + bufferOffset,
                imageSize, imageBarrierList.data() + imageOffset
            };

            cmd.pipelineBarrier2(dependencyInfo);
        };

        if (auto const rCode = cmd.begin({}); rCode < vk::Result::eSuccess)
            return rCode;

        applyBarriers(
            memory.imageBarriers,
            memory.bufferBarriers,
            pass.imagePreBarrierOffset,
            pass.imagePreBarrierSize,
            pass.bufferPreBarrierOffset,
            pass.bufferPreBarrierSize);

        std::invoke(pass.callback, cmd, resourceMap);

        applyBarriers(
            memory.imageBarriers,
            memory.bufferBarriers,
            pass.imagePostBarrierOffset,
            pass.imagePostBarrierSize,
            pass.bufferPostBarrierOffset,
            pass.bufferPostBarrierSize);

        if (auto const rCode = cmd.end(); rCode < vk::Result::eSuccess)
            return rCode;

        return vk::Result::eSuccess;
    }
}

namespace caldera
{
    std::expected<SubmittableGraph, vk::Result>
    GraphExecutor::execute(
        RenderGraph const &graph,
        ExecutionContext const &context,
        ExecutionMemory &memory)
    {
        ::prepare_execution_memory(memory, graph.m_compiled);

        ::translate_compiled_barriers(memory, graph.m_compiled, *context.resourceMap, *context.stateManager);
        ::copy_pass_dependencies(memory, graph.m_compiled);

        SubmittableGraph result;

        for (auto const& pass : graph.m_compiled.compiledPasses)
        {
            auto const cmd = context.distributor->take_buffer(pass.familyIdx);

            if (auto const rCode = ::process_cmd(cmd, memory, *context.resourceMap, pass);
                rCode < vk::Result::eSuccess)
            {
                return std::unexpected(rCode);
            }

            memory.submittablePasses.emplace_back(pass.familyIdx, cmd,
                std::span{ memory.passDependencies.data() + pass.passDependencyOffset, pass.passDependencySize });
        }

        result.passes = memory.submittablePasses;
        return result;
    }
}


/*
#include <iostream>
#include <vulkan/vulkan.hpp>
#include <caldera/graph_executor.h>
#include <caldera/structs.h>

#include "caldera/command_buffer_distributor.hpp"
#include "../include/caldera/detail/graph_allocator.hpp"
#include "caldera/resource_map.hpp"
#include "caldera/resource_state_manager.h"
#include "caldera/semaphore_map.h"
#include "detail/access_resolver.h"
#include "detail/command_buffer_distributor.hpp"
#include "detail/render_graph.h"
#include "detail/resource_map.hpp"
#include "detail/resource_state_manager.h"
#include "detail/semaphore_map.h"
#include "detail/utility.h"

namespace
{
    template <class TagTy>
    void print_barrier_group(
        uint32_t passIdx,
        auto const& group)
    {
        for (auto const& barrier : group)
        {
            if constexpr (std::same_as<TagTy, caldera::ImageTag>)
                std::cout << "Barrier of IMAGE #" << barrier.image << "\n";
            else
                std::cout << "Barrier of BUFFER #" << barrier.buffer << "\n";

            std::cout << "  Access: " << vk::to_string(barrier.srcAccessMask) << " ==> " << vk::to_string(barrier.dstAccessMask) << "\n";
            std::cout << "  Stages: " << vk::to_string(barrier.srcStageMask) << " ==> " << vk::to_string(barrier.dstStageMask) << "\n";

            if constexpr (std::same_as<TagTy, caldera::ImageTag>)
                std::cout << "  Layout: " << vk::to_string(barrier.oldLayout) << " ==> " << vk::to_string(barrier.newLayout) << "\n";

            std::cout << "  Family: " << barrier.srcQueueFamilyIndex << " ==> " << barrier.dstQueueFamilyIndex << "\n";
        }
    }
}

namespace
{

    struct TranslatedPassBarriers
    {
        std::vector<vk::ImageMemoryBarrier2> imagePreBarriers;
        std::vector<vk::BufferMemoryBarrier2> bufferPreBarriers;

        std::vector<vk::ImageMemoryBarrier2> imagePostBarriers;
        std::vector<vk::BufferMemoryBarrier2> bufferPostBarriers;
    };

    [[nodiscard]] vk::ImageMemoryBarrier2
    translate_one_barrier(
        caldera::ResourceMap const& resourceMap,
        caldera::detail::ImageBarrier const& barrier)
    {
        bool const ignoreQueues = barrier.srcFamily == vk::QueueFamilyIgnored ||
                                  barrier.dstFamily == vk::QueueFamilyIgnored;

        return vk::ImageMemoryBarrier2
        {
            barrier.srcStage,
            barrier.srcAccess,
            barrier.dstStage,
            barrier.dstAccess,
            barrier.oldLayout,
            barrier.newLayout,
            ignoreQueues ? vk::QueueFamilyIgnored : barrier.srcFamily,
            ignoreQueues ? vk::QueueFamilyIgnored : barrier.dstFamily,
            resourceMap.get_handle(barrier.image),
            { resourceMap.get_properties(barrier.image).aspects, 0, vk::RemainingMipLevels, 0, vk::RemainingArrayLayers }
        };
    }

    [[nodiscard]] vk::BufferMemoryBarrier2
    translate_one_barrier(
        caldera::ResourceMap const& resourceMap,
        caldera::detail::BufferBarrier const& barrier)
    {
        bool const ignoreQueues = barrier.srcFamily == vk::QueueFamilyIgnored ||
                                  barrier.dstFamily == vk::QueueFamilyIgnored;

        return vk::BufferMemoryBarrier2
        {
            barrier.srcStage,
            barrier.srcAccess,
            barrier.dstStage,
            barrier.dstAccess,
            ignoreQueues ? vk::QueueFamilyIgnored : barrier.srcFamily,
            ignoreQueues ? vk::QueueFamilyIgnored : barrier.dstFamily,
            resourceMap.get_handle(barrier.buffer),
            0, vk::WholeSize
        };
    }

    [[nodiscard]] std::vector<TranslatedPassBarriers>
    translate_barriers(
        auto const& passBarrierList,
        caldera::ResourceMap const& resourceMap)
    {
        std::vector<TranslatedPassBarriers> result;
        result.reserve(passBarrierList.size());

        auto const processGroup = [&] (auto const& in, auto& out)
        {
            out.reserve(in.size());

            for (auto const& inBarrier : in)
                out.push_back(::translate_one_barrier(resourceMap, inBarrier));
        };

        for (auto const& input : passBarrierList)
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
        caldera::ResourceStateManager const& stateManager,
        caldera::ResourceMap const& resourceMap,
        auto& barriers,
        auto const barrierGroupPtr,
        auto const& trackedInfos)
    {
        for (auto const& [resourceID, info] : trackedInfos)
        {
            auto const handle = resourceMap.get_handle(resourceID);

            auto& group = barriers[info.firstPass].*barrierGroupPtr;
            auto const& lastTouch = caldera::ResourceStateManager::ExecutorAttorney::get_state(stateManager, resourceID);

            caldera::detail::AccessResolver resolver;
            auto const hazard = resolver.has_hazards(lastTouch, info.firstTouch);

            if (hazard == caldera::detail::HazardType::none)
                continue;

            auto const barrier = hazard == caldera::detail::HazardType::access
                ? resolver.resolve_access_hazard(resourceID, lastTouch, info.firstTouch)
                : resolver.resolve_ownership_hazard(resourceID, lastTouch, info.firstTouch)[1];

            auto const translated = ::translate_one_barrier(resourceMap, barrier);
            group.push_back(translated);
        }
    }

    void deploy_swapchain_barriers(
        caldera::ExecutionOptions const& options,
        caldera::ResourceMap const& resourceMap,
        auto const& trackedImageInfos,
        auto& barriers)
    {
        auto const imageHandle = resourceMap.get_handle(options.swapchainImageID);
        auto const& trackedInfo = trackedImageInfos.at(options.swapchainImageID);

        auto& barrierGroup = barriers[trackedInfo.lastPass].imagePostBarriers;
        assert(0 == std::ranges::count(barrierGroup, imageHandle, &vk::ImageMemoryBarrier::image));

        caldera::detail::AccessResolver resolver;

        auto const barrier = resolver.resolve_access_hazard();

        barrierGroup.push_back();
    }

    void apply_barriers(
        vk::CommandBuffer const cmd,
        auto const& imageGroup,
        auto const& bufferGroup)
    {
        if (imageGroup.empty() && bufferGroup.empty())
            return;

        vk::DependencyInfo info {};

        if (!imageGroup.empty())
            info.setImageMemoryBarriers(imageGroup);

        if (!bufferGroup.empty())
            info.setBufferMemoryBarriers(bufferGroup);

        cmd.pipelineBarrier2(info);
    }

    struct TranslatedPassSemaphores
    {
        std::vector<vk::SemaphoreSubmitInfo> waitSemaphores;
        std::vector<vk::SemaphoreSubmitInfo> signalSemaphores;
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
                auto semaphore = caldera::SemaphoreMap
                    ::ExecutorAttorney::get_semaphore(semaphoreMap, family);

                ++semaphore.timeline;

                auto& signalSemaphores = result[otherPassIdx].signalSemaphores;

                if (signalSemaphores.empty())
                    signalSemaphores.emplace_back();

                auto& signal = signalSemaphores.front();

                assert(signal.semaphore == VK_NULL_HANDLE ||
                      (signal.semaphore == semaphore.handle && signal.value == semaphore.timeline + otherPassIdx));

                signal.semaphore = semaphore.handle;
                signal.value = semaphore.timeline + otherPassIdx;
                signal.stageMask |= stages;

                result[passIdx].waitSemaphores.emplace_back(
                   semaphore.handle, semaphore.timeline + otherPassIdx, stages);

                semaphoreMap.update_timeline(family, semaphore.timeline);
            }

        return result;
    }

    void deploy_swapchain_semaphores(
        caldera::ExecutionOptions const& options,
        auto const& imageTrackedInfos,
        auto& semaphores)
    {
        caldera::detail::ImageTrackedInfo const& trackedInfo = imageTrackedInfos.at(options.swapchainImageID);

        semaphores[trackedInfo.firstPass].waitSemaphores.emplace_back(
            options.imageAvailableSemaphore, 0, trackedInfo.firstTouch.stages);

        semaphores[trackedInfo.lastPass].signalSemaphores.emplace_back(
            options.renderFinishedSemaphore, 0, trackedInfo.lastTouch.stages);
    }

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
        ExecutionContext const &context,
        std::optional<ExecutionOptions> const options)
    {
        auto const& compiled = RenderGraph::ExecutorAttorney::get_compiled(graph);
        auto const& allocated = RenderGraph::ExecutorAttorney::get_allocated(graph);

        ResourceMap::TransientAttorney::import_transient(*context.resourceMap, allocated);
        ResourceStateManager::ExecutorAttorney::cleanup_transient(*context.stateManager);

        auto translatedBarriers = ::translate_barriers(compiled.barriers, *context.resourceMap);

        ::deploy_initial_barriers(*context.stateManager, *context.resourceMap,
            translatedBarriers, &::TranslatedPassBarriers::imagePreBarriers, compiled.imageTrackedInfos);

        ::deploy_initial_barriers(*context.stateManager, *context.resourceMap,
            translatedBarriers, &::TranslatedPassBarriers::bufferPreBarriers, compiled.bufferTrackedInfos);

        auto translatedSemaphores = ::translate_semaphores(
            compiled.passes, compiled.barriers, *context.semaphoreMap);

        if (options.has_value()) {
            ::deploy_swapchain_barriers(options.value(), *context.resourceMap, translatedBarriers);
            ::deploy_swapchain_semaphores(options.value(), compiled.imageTrackedInfos, translatedSemaphores);
        }

        SubmittableGraph result;
        result.passes.reserve(compiled.passes.size());

        for (uint32_t passIdx = 0; passIdx < compiled.passes.size(); ++passIdx)
        {
            auto const& passInfo = compiled.passes[passIdx];
            auto const& barriers = translatedBarriers[passIdx];
            auto const& semaphores = translatedSemaphores[passIdx];

            if (1) {
                std::cout << "\n=== In Pass #" << passIdx << " ===\n";
                std::cout << "=- Pre-barriers -=\n";

                ::print_barrier_group<ImageTag>(passIdx, barriers.imagePreBarriers);
                ::print_barrier_group<BufferTag>(passIdx, barriers.bufferPreBarriers);

                std::cout << "=- Post-barriers -=\n";
                ::print_barrier_group<ImageTag>(passIdx, barriers.imagePostBarriers);
                ::print_barrier_group<BufferTag>(passIdx, barriers.bufferPostBarriers);
            }

            auto const cmd = CommandBufferDistributor
                ::ConsumerAttorney::take_buffer(*context.distributor, passInfo.family);

            ::process_cmd(*context.resourceMap, cmd, barriers, passInfo.callback);

            result.passes.emplace_back(
                passInfo.family, cmd, semaphores.signalSemaphores, semaphores.waitSemaphores);
        }

        ::dump_touches(*context.stateManager, compiled.imageTrackedInfos);
        ::dump_touches(*context.stateManager, compiled.bufferTrackedInfos);

        ResourceMap::TransientAttorney::clear_transient(*context.resourceMap);

        return result;
    }
}
*/
