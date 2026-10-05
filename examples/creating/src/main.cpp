#include "vk_context.hpp"
#include "vma_allocator.hpp"
#include "triangle_renderer.h"

#include "caldera/command_buffer_distributor.h"
#include "caldera/graph_allocator.h"
#include "caldera/graph_compiler.h"
#include "caldera/graph_declarator.h"
#include "caldera/graph_executor.h"
#include "caldera/graph_ir.h"
#include "caldera/pass_declarator.h"
#include "caldera/render_graph.h"
#include "caldera/resource_map.h"
#include "caldera/resource_state_manager.h"
#include "caldera/semaphore_map.h"
#include "caldera/structs.h"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

constexpr uint32_t kWidth = 512;
constexpr uint32_t kHeight = 512;
constexpr vk::Format kFormat = vk::Format::eR8G8B8A8Srgb;

bool write_ppm(const char* path, const uint8_t* rgba, uint32_t w, uint32_t h) {
    std::FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    std::fprintf(f, "P6\n%u %u\n255\n", w, h);
    for (size_t i = 0; i < size_t(w) * h; ++i) std::fwrite(rgba + i * 4, 1, 3, f);
    std::fclose(f);
    return true;
}

} // namespace

int main() {
    // ---------- Vulkan ----------
    vkctx::VulkanContext ctx;
    vkctx::ContextConfig cfg;
    cfg.enableValidation = true;
    cfg.minApiVersion = VK_API_VERSION_1_3; // граф использует sync2 (PipelineStageFlags2, SemaphoreSubmitInfo)
    if (!ctx.init(cfg)) return 1;
    if (!ctx.enabled.synchronization2 || !ctx.enabled.dynamicRendering) {
        std::fprintf(stderr, "Нужны synchronization2 и dynamicRendering (Vulkan 1.3)\n");
        return 2;
    }

    vkctx::Allocator alloc; // объявлен после ctx => уничтожится раньше него
    if (!alloc.init(ctx)) return 3;

    auto timeline = ctx.createTimelineSemaphore(0);
    if (!timeline) return 4;

    // По одному командному буферу на пасс (у нас их два). Их раздаёт CommandBufferDistributor.
    auto cbs = ctx.allocateCommandBuffers(ctx.graphics.pool, vk::CommandBufferLevel::ePrimary, 2);
    if (cbs.size() != 2) return 5;

    TriangleRenderer triangle;
    if (!triangle.create(ctx.device, kFormat)) return 6;

    // Внешний (не transient) буфер под результат: граф в него скопирует картинку, мы прочитаем на CPU.
    auto readback = alloc.createReadbackBuffer(vk::DeviceSize(kWidth) * kHeight * 4);
    if (!readback) return 7;

    // ВАЖНО: здесь именно .family (индексы семейств очередей), а не .index (индекс очереди внутри семейства).
    const caldera::FamilyNumbers families{
        .graphics = ctx.graphics.family,
        .transfer = ctx.transfer.family,
        .compute = ctx.compute.family,
    };

    // ---------- Caldera: рантайм-хранилища ----------
    caldera::RenderGraph rg;
    caldera::ResourceMap resourceMap;
    caldera::ResourceStateManager stateManager;
    caldera::SemaphoreMap semaphoreMap;
    caldera::CommandBufferDistributor distributor;
    caldera::GraphAllocator graphAllocator{alloc.handle, families};

    const caldera::BufferID resultBuffer = resourceMap.alias(readback->buffer);
    // stateManager.reset_for(resultBuffer);
    semaphoreMap.map(caldera::FamilyType::graphics, timeline->handle, timeline->lastSignaled);

    /********************* 1. Декларация -> компиляция -> аллокация *********************/
    {
        caldera::GraphDeclarator gdecl;

        const caldera::ImageID target = gdecl.transient_image(caldera::ImageDescription{
            .flags = {},
            .type = vk::ImageType::e2D,
            .format = kFormat,
            .extent = {kWidth, kHeight, 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = vk::SampleCountFlagBits::e1,
            .tiling = vk::ImageTiling::eOptimal,
        });

        caldera::ImageVersion rendered{};

        { // Пасс 1: рисуем треугольник в target
            caldera::PassDeclarator pass{"draw-triangle", caldera::FamilyType::graphics, gdecl};
            rendered = pass.write(target, caldera::ImageUsage::color_attachment);
            pass.set_callback([&triangle, target](caldera::ResourceMap const& res, vk::CommandBuffer cmd) {
                triangle.record_draw(cmd, res.at(target), {kWidth, kHeight});
            });
        }

        { // Пасс 2: копируем результат пасса 1 во внешний буфер
            caldera::PassDeclarator pass{"copy-to-readback", caldera::FamilyType::graphics, gdecl};
            (void)pass.read(rendered, caldera::ImageUsage::transfer);
            (void)pass.write(resultBuffer, caldera::BufferUsage::transfer);
            pass.set_callback([target, resultBuffer](caldera::ResourceMap const& res, vk::CommandBuffer cmd) {
                TriangleRenderer::record_readback(cmd, res.at(target), res.at(resultBuffer), {kWidth, kHeight});
            });
        }

        if (!gdecl.is_complete()) {
            std::fprintf(stderr, "Граф не завершён (is_complete() == false)\n");
            return 8;
        }

        caldera::GraphIR ir = gdecl.bake(ctx.device);

        caldera::GraphCompiler compiler;
        compiler.compile(ir, rg);

        graphAllocator.allocate(ir, rg);
    }

    /********************* 2. Исполнение: граф -> план сабмитов *********************/
    distributor.feed(caldera::FamilyType::graphics, cbs);

    caldera::GraphExecutor executor;
    const caldera::ExecutionContext execCtx{
        .families = families,
        .resourceMap = &resourceMap,
        .stateManager = &stateManager,
        .semaphoreMap = &semaphoreMap,
        .distributor = &distributor,
    };
    const caldera::SubmittableGraph submittable = executor.execute(rg, execCtx);

    /********************* 3. Реальный сабмит (граф сам не сабмитит) *********************/
    auto queue_for = [&](caldera::FamilyType family) -> vk::Queue {
        switch (family) {
            case caldera::FamilyType::transfer: return ctx.transfer.queue;
            case caldera::FamilyType::compute:  return ctx.compute.queue;
            default:                            return ctx.graphics.queue;
        }
    };

    for (const caldera::SubmittablePass& pass : submittable.passes) {
        vk::CommandBufferSubmitInfo cmdInfo;
        cmdInfo.setCommandBuffer(pass.cmd);

        vk::SubmitInfo2 submit;
        submit.setWaitSemaphoreInfos(pass.waitPasses)
            .setCommandBufferInfos(cmdInfo)
            .setSignalSemaphoreInfos(pass.signalPasses);

        if (queue_for(pass.family).submit2(submit) != vk::Result::eSuccess) {
            std::fprintf(stderr, "submit2 failed\n");
            return 9;
        }
    }

    /********************* 4. Читаем результат *********************/
    std::vector<uint8_t> pixels(size_t(kWidth) * kHeight * 4);
    if (!alloc.download(*readback, pixels.data(), pixels.size())) return 11;

    const uint8_t* c = &pixels[(size_t(kHeight / 2 + 80) * kWidth + kWidth / 2) * 4];
    std::printf("pixel inside triangle: %u %u %u %u\n", c[0], c[1], c[2], c[3]);
    if (write_ppm("triangle.ppm", pixels.data(), kWidth, kHeight)) std::printf("saved triangle.ppm\n");

    /********************* 5. Уборка (порядок важен) *********************/
    (void)ctx.device.waitIdle();

    distributor.reset(caldera::FamilyType::graphics);
    resourceMap.unmap_all();
    graphAllocator.deallocate(rg);

    ctx.device.freeCommandBuffers(ctx.graphics.pool, cbs);
    triangle.destroy();
    alloc.destroy(*readback);
    ctx.destroyTimelineSemaphore(*timeline);

    return 0;
}