#include <caldera/command_buffer_distributor.hpp>
#include <caldera/graph_executor.hpp>

#include "caldera/detail/access_resolver.hpp"

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

[[nodiscard]] vk::ImageMemoryBarrier2 translate_one_barrier(caldera::detail::ImageBarrier const &barrier,
                                                            caldera::ResourceMap const &resourceMap) {
    auto const handle = resourceMap.at(barrier.id);

    return vk::ImageMemoryBarrier2{barrier.srcStages,       barrier.srcAccess,    barrier.dstStages,
                                   barrier.dstAccess,       barrier.oldLayout,    barrier.newLayout,
                                   barrier.srcFamilyIdx,    barrier.dstFamilyIdx, handle,
                                   barrier.subresourceRange};
}

[[nodiscard]] vk::BufferMemoryBarrier2 translate_one_barrier(caldera::detail::BufferBarrier const &barrier,
                                                             caldera::ResourceMap const &resourceMap) {
    auto const handle = resourceMap.at(barrier.id);

    return vk::BufferMemoryBarrier2{barrier.srcStages,
                                    barrier.srcAccess,
                                    barrier.dstStages,
                                    barrier.dstAccess,
                                    barrier.srcFamilyIdx,
                                    barrier.dstFamilyIdx,
                                    handle,
                                    0,
                                    vk::WholeSize};
}

[[nodiscard]] std::optional<vk::ImageMemoryBarrier2>
generate_one_initial_barrier(caldera::ImageID const image, caldera::detail::ImageTrackedInfo const &trackedInfo,
                             caldera::ResourceStateManager const &stateManager, caldera::ResourceMap const &resourceMap,
                             caldera::FamilyNumbers const &families) {
    auto const handle = resourceMap.at(image);
    auto const srcState = stateManager.get_last_state(image);
    auto const dstTate = translate_to_state(trackedInfo.firstTouch, families);

    caldera::detail::AccessResolver constexpr resolver{};

    if (auto const hazard = resolver.has_hazards(srcState, dstTate); hazard != caldera::detail::HazardType::none) {
        vk::ImageMemoryBarrier2 result{};

        if (hazard == caldera::detail::HazardType::access) {
            result.srcStageMask = srcState.stages;
            result.srcAccessMask = srcState.access;

            result.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
            result.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
        } else {
            result.srcQueueFamilyIndex = srcState.familyIdx;
            result.dstQueueFamilyIndex = dstTate.familyIdx;
        }

        result.dstStageMask = dstTate.stages;
        result.dstAccessMask = dstTate.access;
        result.oldLayout = srcState.layout;
        result.newLayout = dstTate.layout;
        result.image = handle;
        result.subresourceRange = {trackedInfo.aspect, 0, vk::RemainingMipLevels, 0, vk::RemainingArrayLayers};

        return result;
    }

    return std::nullopt;
}

[[nodiscard]] std::optional<vk::BufferMemoryBarrier2>
generate_one_initial_barrier(caldera::BufferID const buffer, caldera::detail::BufferTrackedInfo const &trackedInfo,
                             caldera::ResourceStateManager const &stateManager, caldera::ResourceMap const &resourceMap,
                             caldera::FamilyNumbers const &families) {
    auto const handle = resourceMap.at(buffer);
    auto const srcState = stateManager.get_last_state(buffer);
    auto const dstTate = translate_to_state(trackedInfo.firstTouch, families);

    caldera::detail::AccessResolver constexpr resolver{};

    if (auto const hazard = resolver.has_hazards(srcState, dstTate); hazard != caldera::detail::HazardType::none) {
        vk::BufferMemoryBarrier2 result{};

        if (hazard == caldera::detail::HazardType::access) {
            result.srcStageMask = srcState.stages;
            result.srcAccessMask = srcState.access;

            result.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
            result.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
        } else {
            result.srcQueueFamilyIndex = srcState.familyIdx;
            result.dstQueueFamilyIndex = dstTate.familyIdx;
        }

        result.dstStageMask = dstTate.stages;
        result.dstAccessMask = dstTate.access;
        result.buffer = handle;
        result.offset = 0;
        result.size = vk::WholeSize;

        return result;
    }

    return std::nullopt;
}
} // namespace

namespace {
void prepare_execution_memory(caldera::ExecutionMemory &memory, caldera::detail::CompiledGraph const &graph) {
    memory.imageTranslatedBarriers.clear();
    memory.bufferTranslatedBarriers.clear();

    memory.imageInitBarriers.clear();
    memory.bufferInitBarriers.clear();

    memory.passDependencies.clear();
    memory.submittablePasses.clear();

    memory.imageTranslatedBarriers.reserve(graph.imageBarriers.size() + graph.trackedImages.size());
    memory.bufferTranslatedBarriers.reserve(graph.bufferBarriers.size() + graph.trackedBuffers.size());

    memory.imageInitBarriers.reserve(graph.initImages.size());
    memory.bufferInitBarriers.reserve(graph.initBuffers.size());

    memory.passDependencies.reserve(graph.passDependencies.size());
    memory.submittablePasses.reserve(graph.compiledPasses.size());
}

void translate_compiled_barriers(caldera::ExecutionMemory &memory, caldera::detail::CompiledGraph const &graph,
                                 caldera::ResourceMap const &resourceMap) {
    memory.imageTranslatedBarriers.resize(graph.imageBarriers.size());
    memory.bufferTranslatedBarriers.resize(graph.bufferBarriers.size());

    auto const func = [&](auto const &barrier) { return ::translate_one_barrier(barrier, resourceMap); };

    std::ranges::transform(graph.imageBarriers, memory.imageTranslatedBarriers.begin(), func);
    std::ranges::transform(graph.bufferBarriers, memory.bufferTranslatedBarriers.begin(), func);
}

void generate_initial_barriers_for_pass(caldera::ExecutionMemory &memory, caldera::detail::CompiledGraph const &graph,
                                        caldera::ResourceStateManager const &stateManager,
                                        caldera::ResourceMap const &resourceMap, caldera::FamilyNumbers const &families,
                                        std::span<caldera::ImageID const> const initImages,
                                        std::span<caldera::BufferID const> const initBuffers) {
    auto const generateFor = [&](auto const resources, auto const &trackingTable, auto &barrierList) {
        barrierList.clear();
        assert(barrierList.capacity() >= resources.size());

        for (auto const id : resources) {
            auto const &trackedInfo = trackingTable.at(id);

            if (auto const barrier =
                    ::generate_one_initial_barrier(id, trackedInfo, stateManager, resourceMap, families))
                barrierList.push_back(barrier.value());
        }
    };

    generateFor(initImages, graph.trackedImages, memory.imageInitBarriers);
    generateFor(initBuffers, graph.trackedBuffers, memory.bufferInitBarriers);
}

void copy_pass_dependencies(caldera::ExecutionMemory &memory, caldera::detail::CompiledGraph const &graph) {
    memory.passDependencies.assign(graph.passDependencies.begin(), graph.passDependencies.end());
}

[[nodiscard]] vk::Result process_cmd(vk::CommandBuffer const cmd, caldera::ExecutionMemory const &memory,
                                     caldera::ResourceMap const &resourceMap,
                                     caldera::detail::CompiledPass const &pass) {
    auto const applyInitBarriers = [&] {
        vk::DependencyInfo const dependencyInfo{{},
                                                0,
                                                nullptr,
                                                static_cast<uint32_t>(memory.bufferInitBarriers.size()),
                                                memory.bufferInitBarriers.data(),
                                                static_cast<uint32_t>(memory.imageInitBarriers.size()),
                                                memory.imageInitBarriers.data()};

        cmd.pipelineBarrier2(dependencyInfo);
    };

    auto const applyBakedBarriers = [&](auto const &imageBarrierList, auto const &bufferBarrierList,
                                        uint32_t const imageOffset, uint32_t const imageSize,
                                        uint32_t const bufferOffset, uint32_t const bufferSize) {
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

    applyInitBarriers();
    applyBakedBarriers(memory.imageTranslatedBarriers, memory.bufferTranslatedBarriers, pass.imagePreBarrierOffset,
                       pass.imagePreBarrierSize, pass.bufferPreBarrierOffset, pass.bufferPreBarrierSize);

    std::invoke(pass.callback, cmd, resourceMap);

    applyBakedBarriers(memory.imageTranslatedBarriers, memory.bufferTranslatedBarriers, pass.imagePostBarrierOffset,
                       pass.imagePostBarrierSize, pass.bufferPostBarrierOffset, pass.bufferPostBarrierSize);

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
        std::cout << "=- Dynamic init-barriers -=\n";

        ::print_barrier_group<caldera::ImageTag>(memory.imageInitBarriers);
        ::print_barrier_group<caldera::BufferTag>(memory.bufferInitBarriers);

        std::cout << "=- Baked pre-barriers -=\n";
        ::print_barrier_group<caldera::ImageTag>(
            std::span{memory.imageTranslatedBarriers.data() + pass.imagePreBarrierOffset, pass.imagePreBarrierSize});
        ::print_barrier_group<caldera::BufferTag>(
            std::span{memory.bufferTranslatedBarriers.data() + pass.bufferPreBarrierOffset, pass.bufferPreBarrierSize});

        std::cout << "=- Baked post-barriers -=\n";
        ::print_barrier_group<caldera::ImageTag>(
            std::span{memory.imageTranslatedBarriers.data() + pass.imagePostBarrierOffset, pass.imagePostBarrierSize});
        ::print_barrier_group<caldera::BufferTag>(std::span{
            memory.bufferTranslatedBarriers.data() + pass.bufferPostBarrierOffset, pass.bufferPostBarrierSize});
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

    ::translate_compiled_barriers(memory, graph.m_compiled, *context.resourceMap);
    ::copy_pass_dependencies(memory, graph.m_compiled);

#ifndef NDEBUG
    ::print_all_barriers(memory, graph.m_compiled);
#endif

    SubmittableGraph result;

    for (auto const &pass : graph.m_compiled.compiledPasses) {
        ::generate_initial_barriers_for_pass(
            memory, graph.m_compiled, *context.stateManager, *context.resourceMap, graph.m_families,
            std::span{graph.m_compiled.initImages.begin() + pass.initImageOffset, pass.initImageSize},
            std::span{graph.m_compiled.initBuffers.begin() + pass.initBufferOffset, pass.initBufferSize});

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