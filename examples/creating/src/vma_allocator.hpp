#pragma once
// Обёртка над VmaAllocator поверх vkctx::VulkanContext (сам контекст не меняется).
// Статическая линковка: VMA берёт функции Vulkan напрямую из libvulkan.
// ВАЖНО: Allocator должен умирать РАНЬШЕ VulkanContext (объявляйте его после контекста).

#include "vk_context.hpp"

// Эти макросы читаются только реализацией, но держим их здесь для согласованности.
#ifndef VMA_STATIC_VULKAN_FUNCTIONS
#define VMA_STATIC_VULKAN_FUNCTIONS 1
#endif
#ifndef VMA_DYNAMIC_VULKAN_FUNCTIONS
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#endif
#ifndef VMA_VULKAN_VERSION
#define VMA_VULKAN_VERSION 1003000
#endif
#include "vk_mem_alloc.h"

#include <cstdint>
#include <optional>
#include <string>

namespace vkctx {

struct Buffer {
    vk::Buffer buffer{};
    VmaAllocation allocation{};
    VmaAllocationInfo info{};  // info.pMappedData != nullptr, если создан с MAPPED
    vk::DeviceSize size = 0;

    void* mapped() const { return info.pMappedData; }
    explicit operator bool() const { return static_cast<bool>(buffer); }
};

struct Image {
    vk::Image image{};
    VmaAllocation allocation{};
    VmaAllocationInfo info{};
    vk::Extent3D extent{};
    vk::Format format = vk::Format::eUndefined;
    uint32_t mipLevels = 1;
    uint32_t arrayLayers = 1;

    explicit operator bool() const { return static_cast<bool>(image); }
};

class Allocator {
public:
    Allocator() = default;
    ~Allocator() { shutdown(); }
    Allocator(const Allocator&) = delete;
    Allocator& operator=(const Allocator&) = delete;

    // ctx должен жить дольше аллокатора.
    bool init(const VulkanContext& ctx, VmaAllocatorCreateFlags extraFlags = 0);
    void shutdown();

    // ---- Сам хендл ----
    VmaAllocator handle{};
    operator VmaAllocator() const { return handle; } // можно передавать прямо в vma* функции

    // ---- Буферы ----
    // Полностью ручной вариант
    std::optional<Buffer> createBuffer(const vk::BufferCreateInfo& bci,
                                       const VmaAllocationCreateInfo& aci) const;
    // Только GPU (AUTO_PREFER_DEVICE). TRANSFER_DST добавляется автоматически.
    std::optional<Buffer> createDeviceBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage) const;
    // Host -> GPU: TRANSFER_SRC, persistently mapped, последовательная запись
    std::optional<Buffer> createStagingBuffer(vk::DeviceSize size) const;
    // GPU -> Host: TRANSFER_DST, persistently mapped, произвольное чтение
    std::optional<Buffer> createReadbackBuffer(vk::DeviceSize size) const;
    // Host-видимый буфер с любым usage (uniform/storage и т.п.), persistently mapped
    std::optional<Buffer> createHostBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage) const;
    void destroy(Buffer& b) const;

    // memcpy в mapped-память + flush (для некогерентной памяти). Если буфер не mapped — мапит временно.
    bool upload(const Buffer& b, const void* data, vk::DeviceSize size, vk::DeviceSize offset = 0) const;
    bool download(const Buffer& b, void* out, vk::DeviceSize size, vk::DeviceSize offset = 0) const; // с invalidate
    // Адрес для buffer device address (0, если фича не включена или usage без ShaderDeviceAddress)
    vk::DeviceAddress bufferAddress(const Buffer& b) const;

    // ---- Изображения ----
    std::optional<Image> createImage(const vk::ImageCreateInfo& ici,
                                     const VmaAllocationCreateInfo& aci) const;
    // Удобный 2D device-local образ. dedicated=true — отдельный VkDeviceMemory (рендер-таргеты).
    std::optional<Image> createImage2D(uint32_t width, uint32_t height, vk::Format format,
                                       vk::ImageUsageFlags usage, uint32_t mipLevels = 1,
                                       vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1,
                                       uint32_t arrayLayers = 1, bool dedicated = false) const;
    void destroy(Image& i) const;

    // ---- Прочее ----
    bool flush(VmaAllocation a, vk::DeviceSize offset = 0, vk::DeviceSize size = VK_WHOLE_SIZE) const;
    bool invalidate(VmaAllocation a, vk::DeviceSize offset = 0, vk::DeviceSize size = VK_WHOLE_SIZE) const;
    std::string statsString(bool detailed = false) const; // JSON, удобно смотреть расход памяти графом

    const VulkanContext* context() const { return ctx_; }

private:
    const VulkanContext* ctx_ = nullptr;
};

} // namespace vkctx
