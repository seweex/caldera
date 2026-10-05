#pragma once
// Всё "ручное" Vulkan-кодирование для примера с треугольником вынесено сюда,
// чтобы main.cpp показывал только работу с графом.
// Требует Vulkan 1.3 (dynamic rendering). Без исключений: ошибки -> bool + stderr.

#include <vulkan/vulkan.hpp>

#include <array>
#include <cstdio>
#include <vector>

#include "triangle_spv.h"

class TriangleRenderer {
public:
    TriangleRenderer() = default;
    ~TriangleRenderer() { destroy(); }
    TriangleRenderer(const TriangleRenderer&) = delete;
    TriangleRenderer& operator=(const TriangleRenderer&) = delete;

    bool create(vk::Device device, vk::Format colorFormat) {
        device_ = device;
        format_ = colorFormat;

        auto makeModule = [&](const uint32_t* code, size_t bytes, vk::ShaderModule& out) {
            vk::ShaderModuleCreateInfo ci;
            ci.setCodeSize(bytes).setPCode(code);
            auto r = device_.createShaderModule(ci);
            if (r.result != vk::Result::eSuccess) return false;
            out = r.value;
            return true;
        };
        if (!makeModule(kTriangleVertSpv, sizeof(kTriangleVertSpv), vs_) ||
            !makeModule(kTriangleFragSpv, sizeof(kTriangleFragSpv), fs_)) {
            std::fprintf(stderr, "[triangle] createShaderModule failed\n");
            return false;
        }

        {
            auto r = device_.createPipelineLayout(vk::PipelineLayoutCreateInfo{});
            if (r.result != vk::Result::eSuccess) return false;
            layout_ = r.value;
        }

        std::array<vk::PipelineShaderStageCreateInfo, 2> stages;
        stages[0].setStage(vk::ShaderStageFlagBits::eVertex).setModule(vs_).setPName("main");
        stages[1].setStage(vk::ShaderStageFlagBits::eFragment).setModule(fs_).setPName("main");

        vk::PipelineVertexInputStateCreateInfo vertexInput;  // вершины генерируются в шейдере
        vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
        inputAssembly.setTopology(vk::PrimitiveTopology::eTriangleList);
        vk::PipelineViewportStateCreateInfo viewport;
        viewport.setViewportCount(1).setScissorCount(1);  // сами значения — динамические
        vk::PipelineRasterizationStateCreateInfo raster;
        raster.setPolygonMode(vk::PolygonMode::eFill)
            .setCullMode(vk::CullModeFlagBits::eNone)
            .setFrontFace(vk::FrontFace::eCounterClockwise)
            .setLineWidth(1.0f);
        vk::PipelineMultisampleStateCreateInfo multisample;
        multisample.setRasterizationSamples(vk::SampleCountFlagBits::e1);
        vk::PipelineColorBlendAttachmentState blendAttachment;
        blendAttachment.setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                          vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
        vk::PipelineColorBlendStateCreateInfo blend;
        blend.setAttachments(blendAttachment);
        std::array<vk::DynamicState, 2> dynamicStates{vk::DynamicState::eViewport, vk::DynamicState::eScissor};
        vk::PipelineDynamicStateCreateInfo dynamic;
        dynamic.setDynamicStates(dynamicStates);

        vk::PipelineRenderingCreateInfo rendering;
        rendering.setColorAttachmentFormats(format_);

        vk::GraphicsPipelineCreateInfo gp;
        gp.setPNext(&rendering)
            .setStages(stages)
            .setPVertexInputState(&vertexInput)
            .setPInputAssemblyState(&inputAssembly)
            .setPViewportState(&viewport)
            .setPRasterizationState(&raster)
            .setPMultisampleState(&multisample)
            .setPColorBlendState(&blend)
            .setPDynamicState(&dynamic)
            .setLayout(layout_);

        auto r = device_.createGraphicsPipeline({}, gp);
        if (r.result != vk::Result::eSuccess) {
            std::fprintf(stderr, "[triangle] createGraphicsPipeline failed: %s\n", vk::to_string(r.result).c_str());
            return false;
        }
        pipeline_ = r.value;
        return true;
    }

    // Вызывать ПОСЛЕ завершения работы GPU (вьюхи живут, пока GPU может их использовать).
    void destroy() {
        if (!device_) return;
        for (auto v : views_) device_.destroyImageView(v);
        views_.clear();
        if (pipeline_) device_.destroyPipeline(pipeline_);
        if (layout_) device_.destroyPipelineLayout(layout_);
        if (vs_) device_.destroyShaderModule(vs_);
        if (fs_) device_.destroyShaderModule(fs_);
        pipeline_ = vk::Pipeline{};
        layout_ = vk::PipelineLayout{};
        vs_ = fs_ = vk::ShaderModule{};
        device_ = vk::Device{};
    }

    // Рисует треугольник. image должен уже быть в layout eColorAttachmentOptimal
    // (переход делает граф). Очищает таргет и рисует.
    bool record_draw(vk::CommandBuffer cmd, vk::Image image, vk::Extent2D extent) {
        vk::ImageViewCreateInfo ivci;
        ivci.setImage(image)
            .setViewType(vk::ImageViewType::e2D)
            .setFormat(format_)
            .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
        auto v = device_.createImageView(ivci);
        if (v.result != vk::Result::eSuccess) {
            std::fprintf(stderr, "[triangle] createImageView failed\n");
            return false;
        }
        views_.push_back(v.value);

        vk::RenderingAttachmentInfo color;
        color.setImageView(v.value)
            .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setClearValue(vk::ClearColorValue(std::array<float, 4>{0.05f, 0.05f, 0.08f, 1.0f}));

        vk::RenderingInfo info;
        info.setRenderArea({{0, 0}, extent}).setLayerCount(1).setColorAttachments(color);

        const vk::Viewport viewport{0.0f, 0.0f, float(extent.width), float(extent.height), 0.0f, 1.0f};
        const vk::Rect2D scissor{{0, 0}, extent};

        cmd.beginRendering(info);
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline_);
        cmd.setViewport(0, viewport);
        cmd.setScissor(0, scissor);
        cmd.draw(3, 1, 0, 0);
        cmd.endRendering();
        return true;
    }

    // image должен быть в layout eTransferSrcOptimal.
    static void record_readback(vk::CommandBuffer cmd, vk::Image image, vk::Buffer buffer, vk::Extent2D extent) {
        vk::BufferImageCopy region;
        region.setImageSubresource({vk::ImageAspectFlagBits::eColor, 0, 0, 1})
            .setImageExtent({extent.width, extent.height, 1});
        cmd.copyImageToBuffer(image, vk::ImageLayout::eTransferSrcOptimal, buffer, region);
    }

private:
    vk::Device device_{};
    vk::Format format_ = vk::Format::eUndefined;
    vk::ShaderModule vs_{}, fs_{};
    vk::PipelineLayout layout_{};
    vk::Pipeline pipeline_{};
    std::vector<vk::ImageView> views_;
};
