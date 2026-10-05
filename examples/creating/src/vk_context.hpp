#pragma once
// Базовая инициализация Vulkan (Vulkan-HPP, C++20, без исключений, статическая линковка с libvulkan).
// Все хендлы — публичные поля VulkanContext, так что достать можно что угодно.

#ifndef VULKAN_HPP_NO_EXCEPTIONS
#define VULKAN_HPP_NO_EXCEPTIONS
#endif
// В nothrow-режиме HPP по умолчанию assert-ит на любой ошибке — гасим, ошибки обрабатываем сами.
#ifndef VULKAN_HPP_ASSERT_ON_RESULT
#define VULKAN_HPP_ASSERT_ON_RESULT(x) ((void)(x))
#endif

#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace vkctx {

struct ContextConfig {
    std::string appName = "RenderGraphTest";
    bool enableValidation = true;
    bool preferDiscreteGpu = true;
    uint32_t minApiVersion = VK_API_VERSION_1_2; // 1.2 нужен ради timeline semaphores в ядре

    std::vector<const char*> instanceExtensions; // напр. из glfwGetRequiredInstanceExtensions
    std::vector<const char*> deviceExtensions;   // напр. VK_KHR_swapchain

    // Необязательно: если нужен present (окно). Для headless-тестов оставить пустым.
    std::function<vk::SurfaceKHR(vk::Instance)> createSurface;
};

// Роль очереди + её собственный CommandPool.
// Если выделенной семьи нет, роль делит семью (и vk::Queue) с graphics.
struct QueueInfo {
    vk::Queue queue{};
    uint32_t family = UINT32_MAX;
    uint32_t index = 0;
    vk::CommandPool pool{}; // RESET_COMMAND_BUFFER; пул НЕ потокобезопасен

    bool valid() const { return family != UINT32_MAX; }
};

struct TimelineSemaphore {
    vk::Semaphore handle{};
    uint64_t lastSignaled = 0; // CPU-сторонний счётчик для выдачи следующих значений
    uint64_t nextValue() { return ++lastSignaled; }
};

struct TimelinePoint {
    vk::Semaphore sem{};
    uint64_t value = 0;
    vk::PipelineStageFlags stage = vk::PipelineStageFlagBits::eAllCommands; // для wait
};

struct EnabledFeatures {
    bool timelineSemaphore = false;
    bool bufferDeviceAddress = false;
    bool descriptorIndexing = false;
    bool synchronization2 = false; // только если устройство >= 1.3
    bool dynamicRendering = false; // только если устройство >= 1.3
};

class VulkanContext {
public:
    VulkanContext() = default;
    ~VulkanContext() { shutdown(); }
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    bool init(const ContextConfig& cfg = {});
    void shutdown();

    // ---- Хендлы (всё публично, забирайте что нужно) ----
    vk::Instance instance{};
    vk::DebugUtilsMessengerEXT debugMessenger{};
    vk::SurfaceKHR surface{};
    vk::PhysicalDevice physicalDevice{};
    vk::Device device{};

    vk::PhysicalDeviceProperties properties{};
    vk::PhysicalDeviceMemoryProperties memoryProperties{};
    uint32_t instanceApiVersion = 0;
    uint32_t deviceApiVersion = 0;
    EnabledFeatures enabled{};

    QueueInfo graphics;
    QueueInfo compute;
    QueueInfo transfer;
    QueueInfo present; // valid только если задан createSurface

    // ---- Хелперы ----
    std::optional<vk::CommandPool> createCommandPool(
        uint32_t family,
        vk::CommandPoolCreateFlags flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer) const;

    std::vector<vk::CommandBuffer> allocateCommandBuffers(
        vk::CommandPool pool, vk::CommandBufferLevel level, uint32_t count) const;

    std::optional<vk::Semaphore> createBinarySemaphore() const;
    std::optional<vk::Fence> createFence(bool signaled = false) const;

    std::optional<TimelineSemaphore> createTimelineSemaphore(uint64_t initialValue = 0) const;
    void destroyTimelineSemaphore(TimelineSemaphore& s) const;
    bool waitTimeline(const TimelineSemaphore& s, uint64_t value, uint64_t timeoutNs = UINT64_MAX) const;
    bool signalTimeline(const TimelineSemaphore& s, uint64_t value) const; // сигнал с CPU
    std::optional<uint64_t> timelineValue(const TimelineSemaphore& s) const;

    // Сабмит с timeline-ожиданиями/сигналами (vkQueueSubmit + TimelineSemaphoreSubmitInfo).
    // Бинарные семафоры тоже можно передавать: value для них игнорируется драйвером.
    bool submit(vk::Queue queue,
                std::span<const vk::CommandBuffer> cmds,
                std::span<const TimelinePoint> waits = {},
                std::span<const TimelinePoint> signals = {},
                vk::Fence fence = {}) const;

    // Подбор типа памяти (пригодится для тестовых ресурсов графа)
    std::optional<uint32_t> findMemoryType(uint32_t typeBits, vk::MemoryPropertyFlags required) const;

private:
    // Расширения (debug utils) libvulkan не экспортирует — берём через vkGetInstanceProcAddr.
    PFN_vkDestroyDebugUtilsMessengerEXT pfnDestroyMessenger_ = nullptr;

    bool createInstance(const ContextConfig& cfg);
    bool pickPhysicalDevice(const ContextConfig& cfg);
    bool createDevice(const ContextConfig& cfg);
    bool createQueuePools();
};

} // namespace vkctx