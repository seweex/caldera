#include <caldera/command_buffer_distributor.hpp>
#include <caldera/graph_executor.hpp>
#include <caldera/render_graph.hpp>
#include <caldera/resource_map.hpp>
#include <caldera/resource_state_manager.hpp>
#include <caldera/structs.hpp>
#include <expected>
#include <vulkan/vulkan.hpp>

#ifndef NDEBUG
#include <iostream>
#endif

namespace {
[[nodiscard]] vk::ImageMemoryBarrier2 translate_one_barrier(caldera::detail::ImageBarrier const &barrier,
                                                            caldera::ResourceMap const &resourceMap,
                                                            caldera::ResourceStateManager const &stateManager) {
    auto const handle = resourceMap.at(barrier.id);

    if (barrier.srcFamilyIdx == vk::QueueFamilyIgnored && barrier.dstFamilyIdx != vk::QueueFamilyIgnored) {
        auto const lastState = stateManager.get_last_state(barrier.id);

        if (lastState.familyIdx == vk::QueueFamilyIgnored)
            return vk::ImageMemoryBarrier2{vk::PipelineStageFlagBits2::eAllCommands,
                                           vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite,
                                           barrier.dstStages,
                                           barrier.dstAccess,
                                           barrier.oldLayout,
                                           barrier.newLayout,
                                           vk::QueueFamilyIgnored,
                                           vk::QueueFamilyIgnored,
                                           handle,
                                           barrier.subresourceRange};
        else if (lastState.familyIdx != barrier.dstFamilyIdx)
            return vk::ImageMemoryBarrier2{{},
                                           {},
                                           barrier.dstStages,
                                           barrier.dstAccess,
                                           lastState.layout,
                                           barrier.newLayout,
                                           lastState.familyIdx,
                                           barrier.dstFamilyIdx,
                                           handle,
                                           barrier.subresourceRange};
        else
            return vk::ImageMemoryBarrier2{lastState.stages,        lastState.access,       barrier.dstStages,
                                           barrier.dstAccess,       lastState.layout,       barrier.newLayout,
                                           vk::QueueFamilyIgnored,  vk::QueueFamilyIgnored, handle,
                                           barrier.subresourceRange};
    } else
        return vk::ImageMemoryBarrier2{barrier.srcStages,       barrier.srcAccess,    barrier.dstStages,
                                       barrier.dstAccess,       barrier.oldLayout,    barrier.newLayout,
                                       barrier.srcFamilyIdx,    barrier.dstFamilyIdx, handle,
                                       barrier.subresourceRange};
}

[[nodiscard]] vk::BufferMemoryBarrier2 translate_one_barrier(caldera::detail::BufferBarrier const &barrier,
                                                             caldera::ResourceMap const &resourceMap,
                                                             caldera::ResourceStateManager const &stateManager) {
    auto const handle = resourceMap.at(barrier.id);

    if (barrier.srcFamilyIdx == vk::QueueFamilyIgnored && barrier.dstFamilyIdx != vk::QueueFamilyIgnored) {
        auto const lastState = stateManager.get_last_state(barrier.id);

        if (lastState.familyIdx == vk::QueueFamilyIgnored)
            return vk::BufferMemoryBarrier2{vk::PipelineStageFlagBits2::eAllCommands,
                                            vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite,
                                            barrier.dstStages,
                                            barrier.dstAccess,
                                            vk::QueueFamilyIgnored,
                                            vk::QueueFamilyIgnored,
                                            handle,
                                            barrier.offset,
                                            barrier.size};
        else if (lastState.familyIdx != barrier.dstFamilyIdx)
            return vk::BufferMemoryBarrier2{{},
                                            {},
                                            barrier.dstStages,
                                            barrier.dstAccess,
                                            lastState.familyIdx,
                                            barrier.dstFamilyIdx,
                                            handle,
                                            barrier.offset,
                                            barrier.size};
        else
            return vk::BufferMemoryBarrier2{
                lastState.stages,       lastState.access, barrier.dstStages, barrier.dstAccess, vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored, handle,           barrier.offset,    barrier.size};
    } else
        return vk::BufferMemoryBarrier2{barrier.srcStages,
                                        barrier.srcAccess,
                                        barrier.dstStages,
                                        barrier.dstAccess,
                                        barrier.srcFamilyIdx,
                                        barrier.dstFamilyIdx,
                                        handle,
                                        barrier.offset,
                                        barrier.size};
}

[[nodiscard]] caldera::ImageState translate_to_state(caldera::detail::ImageTouch const &touch,
                                                     caldera::FamilyNumbers const &families) noexcept {
    return caldera::ImageState{.stages = touch.stages,
                               .access = touch.access,
                               .layout = touch.layout,
                               .familyIdx = families.get(touch.family)};
}

[[nodiscard]] caldera::BufferState translate_to_state(caldera::detail::BufferTouch const &touch,
                                                      caldera::FamilyNumbers const &families) noexcept {
    return caldera::BufferState{
        .stages = touch.stages, .access = touch.access, .familyIdx = families.get(touch.family)};
}
} // namespace

namespace {
void prepare_execution_memory(caldera::ExecutionMemory &memory, caldera::detail::CompiledGraph const &graph) {
    memory.imageBarriers.clear();
    memory.bufferBarriers.clear();

    memory.passDependencies.clear();
    memory.submittablePasses.clear();

    memory.imageBarriers.reserve(graph.imageBarriers.size() + graph.trackedImages.size());
    memory.bufferBarriers.reserve(graph.bufferBarriers.size() + graph.trackedBuffers.size());

    memory.passDependencies.reserve(graph.passDependencies.size());
    memory.submittablePasses.reserve(graph.compiledPasses.size());
}

void translate_compiled_barriers(caldera::ExecutionMemory &memory, caldera::detail::CompiledGraph const &graph,
                                 caldera::ResourceMap const &resourceMap,
                                 caldera::ResourceStateManager const &stateManager) {
    memory.imageBarriers.resize(graph.imageBarriers.size());
    memory.bufferBarriers.resize(graph.bufferBarriers.size());

    auto const func = [&](auto const &barrier) { return ::translate_one_barrier(barrier, resourceMap, stateManager); };

    std::ranges::transform(graph.imageBarriers, memory.imageBarriers.begin(), func);
    std::ranges::transform(graph.bufferBarriers, memory.bufferBarriers.begin(), func);
}

void copy_pass_dependencies(caldera::ExecutionMemory &memory, caldera::detail::CompiledGraph const &graph) {
    memory.passDependencies.assign(graph.passDependencies.begin(), graph.passDependencies.end());
}

[[nodiscard]] vk::Result process_cmd(vk::CommandBuffer const cmd, caldera::ExecutionMemory const &memory,
                                     caldera::ResourceMap const &resourceMap,
                                     caldera::detail::CompiledPass const &pass) {
    auto const applyBarriers = [&](auto const &imageBarrierList, auto const &bufferBarrierList,
                                   uint32_t const imageOffset, uint32_t const imageSize, uint32_t const bufferOffset,
                                   uint32_t const bufferSize) {
        vk::DependencyInfo const dependencyInfo{{},
                                                0,
                                                nullptr,
                                                bufferSize,
                                                bufferBarrierList.data() + bufferOffset,
                                                imageSize,
                                                imageBarrierList.data() + imageOffset};

        cmd.pipelineBarrier2(dependencyInfo);
    };

    if (auto const rCode = cmd.begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
        rCode < vk::Result::eSuccess)
        return rCode;

    applyBarriers(memory.imageBarriers, memory.bufferBarriers, pass.imagePreBarrierOffset, pass.imagePreBarrierSize,
                  pass.bufferPreBarrierOffset, pass.bufferPreBarrierSize);

    std::invoke(pass.callback, cmd, resourceMap);

    applyBarriers(memory.imageBarriers, memory.bufferBarriers, pass.imagePostBarrierOffset, pass.imagePostBarrierSize,
                  pass.bufferPostBarrierOffset, pass.bufferPostBarrierSize);

    if (auto const rCode = cmd.end(); rCode < vk::Result::eSuccess)
        return rCode;

    return vk::Result::eSuccess;
}

template <typename TagTy>
void dump_resource_states(caldera::ResourceStateManager &manager, caldera::detail::CompiledGraph const &graph,
                          caldera::FamilyNumbers const &families) {
    auto const &trackedResources = std::invoke([&]() -> auto const & {
        if constexpr (std::same_as<TagTy, caldera::ImageTag>)
            return graph.trackedImages;
        else
            return graph.trackedBuffers;
    });

    for (auto const &[resourceID, state] : trackedResources)
        if (!resourceID.isTransient) {
            manager.dump(resourceID, ::translate_to_state(state.lastTouch, families));
        }
}
#ifndef NDEBUG
template <class TagTy> void print_barrier_group(auto const &group) {
    for (auto const &barrier : group) {
        if constexpr (std::same_as<TagTy, caldera::ImageTag>)
            std::cout << "Barrier of IMAGE #" << barrier.image << "\n";
        else
            std::cout << "Barrier of BUFFER #" << barrier.buffer << "\n";

        std::cout << "  Access: " << vk::to_string(barrier.srcAccessMask) << " ==> "
                  << vk::to_string(barrier.dstAccessMask) << "\n";
        std::cout << "  Stages: " << vk::to_string(barrier.srcStageMask) << " ==> "
                  << vk::to_string(barrier.dstStageMask) << "\n";

        if constexpr (std::same_as<TagTy, caldera::ImageTag>)
            std::cout << "  Layout: " << vk::to_string(barrier.oldLayout) << " ==> " << vk::to_string(barrier.newLayout)
                      << "\n";

        std::cout << "  Family: " << barrier.srcQueueFamilyIndex << " ==> " << barrier.dstQueueFamilyIndex << "\n";
    }
}

void print_all_barriers(caldera::ExecutionMemory const &memory, caldera::detail::CompiledGraph const &graph) {
    for (uint32_t passIdx = 0; passIdx < graph.compiledPasses.size(); ++passIdx) {
        auto const &pass = graph.compiledPasses[passIdx];

        std::cout << "\n=== In Pass #" << passIdx << " ===\n";
        std::cout << "=- Pre-barriers -=\n";

        ::print_barrier_group<caldera::ImageTag>(
            std::span{memory.imageBarriers.data() + pass.imagePreBarrierOffset, pass.imagePreBarrierSize});
        ::print_barrier_group<caldera::BufferTag>(
            std::span{memory.bufferBarriers.data() + pass.bufferPreBarrierOffset, pass.bufferPreBarrierSize});

        std::cout << "=- Post-barriers -=\n";
        ::print_barrier_group<caldera::ImageTag>(
            std::span{memory.imageBarriers.data() + pass.imagePostBarrierOffset, pass.imagePostBarrierSize});
        ::print_barrier_group<caldera::BufferTag>(
            std::span{memory.bufferBarriers.data() + pass.bufferPostBarrierOffset, pass.bufferPostBarrierSize});
    }
}
#endif
} // namespace

namespace caldera {
std::expected<SubmittableGraph, vk::Result>
GraphExecutor::execute(RenderGraph const &graph, ExecutionContext const &context, ExecutionMemory &memory) {
    assert(graph && "graph must not be empty");
    context.resourceMap->bind_resources(graph.m_resources);

    ::prepare_execution_memory(memory, graph.m_compiled);

    ::translate_compiled_barriers(memory, graph.m_compiled, *context.resourceMap, *context.stateManager);
    ::copy_pass_dependencies(memory, graph.m_compiled);

#ifndef NDEBUG
    ::print_all_barriers(memory, graph.m_compiled);
#endif

    SubmittableGraph result;

    for (auto const &pass : graph.m_compiled.compiledPasses) {

        context.resourceMap->bind_resource_filters(
            std::span{graph.m_compiled.allowedImages.begin() + pass.allowedImageOffset, pass.allowedImageSize},
            std::span{graph.m_compiled.allowedBuffers.begin() + pass.allowedBufferOffset, pass.allowedBufferSize},
            std::span{graph.m_compiled.allowedImageViews.begin() + pass.allowedImageViewOffset,
                      pass.allowedImageViewSize});

        auto const cmd = context.distributor->take_buffer(pass.familyIdx);

        if (auto const rCode = ::process_cmd(cmd, memory, *context.resourceMap, pass); rCode < vk::Result::eSuccess) {
            return std::unexpected(rCode);
        }

        memory.submittablePasses.emplace_back(
            pass.familyIdx, cmd,
            std::span{memory.passDependencies.data() + pass.passDependencyOffset, pass.passDependencySize});
    }

    ::dump_resource_states<ImageTag>(*context.stateManager, graph.m_compiled, graph.m_families);
    ::dump_resource_states<BufferTag>(*context.stateManager, graph.m_compiled, graph.m_families);

    result.passes = std::span{memory.submittablePasses};
    return result;
}
} // namespace caldera