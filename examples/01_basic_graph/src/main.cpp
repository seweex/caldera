#include <iostream>

#include <vulkan/vulkan.hpp>

#include <VkBootstrap.h>
#include <caldera/caldera.h>

#include <framework/allocator.h>
#include <framework/commands.h>
#include <framework/semaphore.h>
#include <framework/view.h>
#include <framework/fence.h>

vkb::QueueType map_queue_type(caldera::FamilyType const type) noexcept
{
    switch (type) {
    case caldera::FamilyType::unknown:  assert(false);
    case caldera::FamilyType::graphics: return vkb::QueueType::graphics;
    case caldera::FamilyType::transfer: return vkb::QueueType::transfer;
    case caldera::FamilyType::compute:  return vkb::QueueType::compute;
    }
}

struct Pixel {
    uint8_t r, g, b, a;
};

int main()
{
    namespace cfw = caldera::framework;

    /* Init Vulkan */

    auto const instanceRet = vkb::InstanceBuilder{}
        .require_api_version(1, 3)
        .set_app_name("Caldera Example #1: Basic Graph")
        .set_app_version(0, 0, 1)
        .use_default_debug_messenger()
        .enable_validation_layers()
    .build();
    if (!instanceRet) return -1;

    VkPhysicalDeviceVulkan12Features features2 {};
    features2.timelineSemaphore = true;

    VkPhysicalDeviceVulkan13Features features3 {};
    features3.synchronization2 = true;
    features3.dynamicRendering = true;

    auto const phDeviceRet = vkb::PhysicalDeviceSelector{ instanceRet.value() }
        .set_minimum_version(1, 3)
        .set_required_features_12(features2)
        .set_required_features_13(features3)
        .defer_surface_initialization()
        .require_dedicated_transfer_queue()
    .select ();
    if (!phDeviceRet) return -2;

    auto const deviceRet = vkb::DeviceBuilder{ phDeviceRet.value() }
    .build();
    if (!deviceRet) return -3;

    auto const device = vk::Device{ deviceRet.value().device };
    std::cout << "Chose device " << phDeviceRet->name << std::endl;

    /* * * * * * * */

    cfw::Allocator allocator{
        instanceRet->instance,
        phDeviceRet->physical_device,
        device,
        deviceRet->instance_version
    };

    cfw::CommandFactory cmdFactory{ device };
    cfw::SemaphoreFactory smfFactory{ device };
    cfw::ImageViewFactory ivFactory{ device };

    auto const gfxQueueIdx = *deviceRet->get_queue_index(vkb::QueueType::graphics);
    auto const trfQueueIdx = *deviceRet->get_queue_index(vkb::QueueType::transfer);

    auto const frameBuff = allocator.make_image({
        {},
        vk::ImageType::e2D,
        vk::Format::eR8G8B8A8Srgb,
        vk::Extent3D{ 1024, 1024, 1 },
        1u,
        1u,
        vk::SampleCountFlagBits::e1,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
        vk::SharingMode::eExclusive,
        1u,
        &gfxQueueIdx,
        vk::ImageLayout::eUndefined
    });

    vk::ImageViewCreateInfo fbViewCreateInfo {};
    fbViewCreateInfo.image = frameBuff.image;
    fbViewCreateInfo.viewType = vk::ImageViewType::e2D;
    fbViewCreateInfo.format = vk::Format::eR8G8B8A8Srgb;
    fbViewCreateInfo.subresourceRange = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

    auto const fbView = ivFactory.create_image_view(fbViewCreateInfo);

    vk::BufferCreateInfo bufferCreateInfo {};
    bufferCreateInfo.size = 2 * 1024 * 1024 * 4;
    bufferCreateInfo.usage = vk::BufferUsageFlagBits::eTransferDst;
    bufferCreateInfo.sharingMode = vk::SharingMode::eExclusive;
    bufferCreateInfo.queueFamilyIndexCount = 1;
    bufferCreateInfo.pQueueFamilyIndices = &trfQueueIdx;

    auto const buffer = allocator.make_buffer(bufferCreateInfo, true);

    auto const gfxBuffers = cmdFactory.create_command_buffers(
        *deviceRet->get_queue_index(vkb::QueueType::graphics), 1);

    auto const trfBuffers = cmdFactory.create_command_buffers(
        *deviceRet->get_queue_index(vkb::QueueType::transfer), 1);

    auto const gfxSemaphore = smfFactory.create_semaphore();
    auto const trfSemaphore = smfFactory.create_semaphore();

    /* * * * * * * */

    caldera::FamilyNumbers const numbers {
        gfxQueueIdx,
        trfQueueIdx,
        *deviceRet->get_queue_index(vkb::QueueType::compute)
    };

    caldera::CommandBufferDistributor cmdDistributor;
    cmdDistributor.feed(caldera::FamilyType::graphics, gfxBuffers.buffers);
    cmdDistributor.feed(caldera::FamilyType::transfer, trfBuffers.buffers);

    caldera::ResourceMap rMap;
    caldera::RenderGraph rg;

    caldera::GraphCompiler rgCompiler;
    caldera::GraphAllocator rgAllocator { allocator.get_allocator(), numbers };

    auto const fb = rMap.alias(frameBuff.image);
    auto const buff = rMap.alias(buffer.buffer);

    /* Build a graph */
    {
        caldera::GraphDeclarator rgDecl;

        caldera::PassDeclarator colorPassDecl{ "color-pass", caldera::FamilyType::graphics, rgDecl };
        auto const fb1 = colorPassDecl.write(fb, caldera::ImageUsage::color_attachment);
        colorPassDecl.set_callback(
            [&] (caldera::ResourceMap const& rm, vk::CommandBuffer const cmd)
            {
                vk::RenderingAttachmentInfo attachmentInfo {};
                attachmentInfo.imageView = fbView.view;
                attachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
                attachmentInfo.loadOp = vk::AttachmentLoadOp::eClear;
                attachmentInfo.storeOp = vk::AttachmentStoreOp::eStore;
                attachmentInfo.clearValue = vk::ClearColorValue{ 1.f, 0.8f, 0.6f, 1.f };

                vk::RenderingInfo renderingInfo {};
                renderingInfo.renderArea = vk::Rect2D{ { 0, 0 }, { 1024, 1024 } };
                renderingInfo.layerCount = 1u;
                renderingInfo.viewMask = 0u;
                renderingInfo.colorAttachmentCount = 1u;
                renderingInfo.pColorAttachments = &attachmentInfo;

                cmd.beginRendering(renderingInfo);



                cmd.endRendering();
            });

        caldera::PassDeclarator copyPassDecl{ "copy-pass", caldera::FamilyType::transfer, rgDecl };
        (void)copyPassDecl.read(fb1, caldera::ImageUsage::transfer);
        auto const buff1 = copyPassDecl.write(buff, caldera::BufferUsage::transfer);
        copyPassDecl.set_callback(
            [&] (caldera::ResourceMap const& rm, vk::CommandBuffer const cmd)
            {
                auto const srcImage = rm.at(fb);
                auto const dstBuffer = rm.at(buff);

                vk::BufferImageCopy region {};
                region.bufferOffset = 0;
                region.bufferRowLength = 0;
                region.bufferImageHeight = 0;

                region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
                region.imageSubresource.mipLevel = 0;
                region.imageSubresource.layerCount = 1;
                region.imageSubresource.baseArrayLayer = 0;

                region.imageExtent = vk::Extent3D{ 1024, 1024, 1 };
                region.imageOffset = vk::Offset3D{ 0, 0, 0 };

                cmd.copyImageToBuffer(srcImage, vk::ImageLayout::eTransferSrcOptimal, dstBuffer, region);
            });

        auto const rgIr = rgDecl.bake(device);

        rgCompiler.compile(rgIr, rg);
        rgAllocator.allocate(rgIr, rg);
    }

    /* Execute Graph */

    cfw::FenceFactory fenceFactory{ device };
    auto const finishFence = fenceFactory.create_fence(false);

    caldera::ResourceStateManager stMgr;
    caldera::SemaphoreMap sMap;

    sMap.map(caldera::FamilyType::graphics, gfxSemaphore.semaphore, gfxSemaphore.timeline);
    sMap.map(caldera::FamilyType::transfer, trfSemaphore.semaphore, trfSemaphore.timeline);

    caldera::GraphExecutor executor;
    caldera::ExecutionContext execCtx {
        .families = numbers,
        .resourceMap = &rMap,
        .stateManager = &stMgr,
        .semaphoreMap = &sMap,
        .distributor = &cmdDistributor
    };

    auto const execRes = executor.execute(rg, execCtx);

    for (auto const& pass : execRes.passes)
    {
        auto const queueType = ::map_queue_type(pass.family);
        auto const queue = vk::Queue{ *deviceRet->get_queue(queueType) };

        vk::CommandBufferSubmitInfo cmdInfo {};
        cmdInfo.commandBuffer = pass.cmd;

        vk::SubmitInfo2 info {};
        info.setCommandBufferInfos(cmdInfo)
        .setWaitSemaphoreInfos(pass.waitPasses)
        .setSignalSemaphoreInfos(pass.signalPasses);

        queue.submit2(info, finishFence.fence);
        (void)device.waitForFences(finishFence.fence, vk::True, UINT64_MAX);
        device.resetFences(finishFence.fence);
    }

    /* Read Result */

    auto const begin = static_cast<Pixel const*>(buffer.mapped);
    auto const end = begin + 1024 * 1024;

    std::vector<Pixel> pixels;
    pixels.assign(begin, end);

    std::cout << "First Pixel Is: " <<
        "r=" << static_cast<uint32_t>(pixels[5].r) << ", " <<
        "g=" << static_cast<uint32_t>(pixels[5].g) << ", " <<
        "b=" << static_cast<uint32_t>(pixels[5].b) << ", " <<
        "a=" << static_cast<uint32_t>(pixels[5].a) << std::endl;

    /* Clear all */

    smfFactory.destroy(trfSemaphore);
    smfFactory.destroy(gfxSemaphore);

    cmdFactory.destroy(trfBuffers);
    cmdFactory.destroy(gfxBuffers);

    allocator.destroy(frameBuff);
    return 0;
}














































