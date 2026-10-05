// Единственная единица трансляции с реализацией VMA.
#define VMA_IMPLEMENTATION
#include "vma_allocator.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace vkctx {
namespace {

bool okVk(VkResult r, const char* what) {
    if (r == VK_SUCCESS) return true;
    std::fprintf(stderr, "[vma] %s failed: %s\n", what, vk::to_string(static_cast<vk::Result>(r)).c_str());
    return false;
}

} // namespace

bool Allocator::init(const VulkanContext& ctx, VmaAllocatorCreateFlags extraFlags) {
    if (handle) return true;
    if (!ctx.device) { std::fprintf(stderr, "[vma] VulkanContext is not initialised\n"); return false; }
    ctx_ = &ctx;

    VmaAllocatorCreateInfo ci{};
    ci.flags = extraFlags;
    if (ctx.enabled.bufferDeviceAddress) ci.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    ci.instance = static_cast<VkInstance>(ctx.instance);
    ci.physicalDevice = static_cast<VkPhysicalDevice>(ctx.physicalDevice);
    ci.device = static_cast<VkDevice>(ctx.device);
    ci.vulkanApiVersion = std::min<uint32_t>(ctx.deviceApiVersion, VK_API_VERSION_1_3);

    if (!okVk(vmaCreateAllocator(&ci, &handle), "vmaCreateAllocator")) {
        handle = nullptr;
        ctx_ = nullptr;
        return false;
    }
    return true;
}

void Allocator::shutdown() {
    if (handle) {
        vmaDestroyAllocator(handle); // все ресурсы должны быть уничтожены к этому моменту
        handle = nullptr;
    }
    ctx_ = nullptr;
}

// ---------------- Буферы ----------------
std::optional<Buffer> Allocator::createBuffer(const vk::BufferCreateInfo& bci,
                                              const VmaAllocationCreateInfo& aci) const {
    Buffer b;
    VkBuffer raw = VK_NULL_HANDLE;
    const VkResult r = vmaCreateBuffer(handle, &static_cast<const VkBufferCreateInfo&>(bci), &aci,
                                       &raw, &b.allocation, &b.info);
    if (!okVk(r, "vmaCreateBuffer")) return std::nullopt;
    b.buffer = vk::Buffer(raw);
    b.size = bci.size;
    return b;
}

std::optional<Buffer> Allocator::createDeviceBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage) const {
    vk::BufferCreateInfo bci;
    bci.setSize(size).setUsage(usage | vk::BufferUsageFlagBits::eTransferDst)
        .setSharingMode(vk::SharingMode::eExclusive);
    VmaAllocationCreateInfo aci{};
    aci.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    return createBuffer(bci, aci);
}

std::optional<Buffer> Allocator::createStagingBuffer(vk::DeviceSize size) const {
    vk::BufferCreateInfo bci;
    bci.setSize(size).setUsage(vk::BufferUsageFlagBits::eTransferSrc)
        .setSharingMode(vk::SharingMode::eExclusive);
    VmaAllocationCreateInfo aci{};
    aci.usage = VMA_MEMORY_USAGE_AUTO;
    aci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    return createBuffer(bci, aci);
}

std::optional<Buffer> Allocator::createReadbackBuffer(vk::DeviceSize size) const {
    vk::BufferCreateInfo bci;
    bci.setSize(size).setUsage(vk::BufferUsageFlagBits::eTransferDst)
        .setSharingMode(vk::SharingMode::eExclusive);
    VmaAllocationCreateInfo aci{};
    aci.usage = VMA_MEMORY_USAGE_AUTO;
    aci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    return createBuffer(bci, aci);
}

std::optional<Buffer> Allocator::createHostBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage) const {
    vk::BufferCreateInfo bci;
    bci.setSize(size).setUsage(usage).setSharingMode(vk::SharingMode::eExclusive);
    VmaAllocationCreateInfo aci{};
    aci.usage = VMA_MEMORY_USAGE_AUTO;
    aci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    return createBuffer(bci, aci);
}

void Allocator::destroy(Buffer& b) const {
    if (b.buffer) vmaDestroyBuffer(handle, static_cast<VkBuffer>(b.buffer), b.allocation);
    b = {};
}

bool Allocator::upload(const Buffer& b, const void* data, vk::DeviceSize size, vk::DeviceSize offset) const {
    if (offset + size > b.size) {
        std::fprintf(stderr, "[vma] upload out of range\n");
        return false;
    }
    void* p = b.info.pMappedData;
    bool mappedHere = false;
    if (!p) {
        if (!okVk(vmaMapMemory(handle, b.allocation, &p), "vmaMapMemory")) return false;
        mappedHere = true;
    }
    std::memcpy(static_cast<char*>(p) + offset, data, static_cast<size_t>(size));
    const bool flushed = flush(b.allocation, offset, size);
    if (mappedHere) vmaUnmapMemory(handle, b.allocation);
    return flushed;
}

bool Allocator::download(const Buffer& b, void* out, vk::DeviceSize size, vk::DeviceSize offset) const {
    if (offset + size > b.size) {
        std::fprintf(stderr, "[vma] download out of range\n");
        return false;
    }
    void* p = b.info.pMappedData;
    bool mappedHere = false;
    if (!p) {
        if (!okVk(vmaMapMemory(handle, b.allocation, &p), "vmaMapMemory")) return false;
        mappedHere = true;
    }
    const bool inv = invalidate(b.allocation, offset, size);
    if (inv) std::memcpy(out, static_cast<const char*>(p) + offset, static_cast<size_t>(size));
    if (mappedHere) vmaUnmapMemory(handle, b.allocation);
    return inv;
}

vk::DeviceAddress Allocator::bufferAddress(const Buffer& b) const {
    if (!ctx_ || !ctx_->enabled.bufferDeviceAddress) return 0;
    vk::BufferDeviceAddressInfo info;
    info.setBuffer(b.buffer);
    return ctx_->device.getBufferAddress(info);
}

// ---------------- Изображения ----------------
std::optional<Image> Allocator::createImage(const vk::ImageCreateInfo& ici,
                                            const VmaAllocationCreateInfo& aci) const {
    Image i;
    VkImage raw = VK_NULL_HANDLE;
    const VkResult r = vmaCreateImage(handle, &static_cast<const VkImageCreateInfo&>(ici), &aci,
                                      &raw, &i.allocation, &i.info);
    if (!okVk(r, "vmaCreateImage")) return std::nullopt;
    i.image = vk::Image(raw);
    i.extent = ici.extent;
    i.format = ici.format;
    i.mipLevels = ici.mipLevels;
    i.arrayLayers = ici.arrayLayers;
    return i;
}

std::optional<Image> Allocator::createImage2D(uint32_t width, uint32_t height, vk::Format format,
                                              vk::ImageUsageFlags usage, uint32_t mipLevels,
                                              vk::SampleCountFlagBits samples, uint32_t arrayLayers,
                                              bool dedicated) const {
    vk::ImageCreateInfo ici;
    ici.setImageType(vk::ImageType::e2D)
        .setFormat(format)
        .setExtent({width, height, 1})
        .setMipLevels(mipLevels)
        .setArrayLayers(arrayLayers)
        .setSamples(samples)
        .setTiling(vk::ImageTiling::eOptimal)
        .setUsage(usage)
        .setSharingMode(vk::SharingMode::eExclusive)
        .setInitialLayout(vk::ImageLayout::eUndefined);
    VmaAllocationCreateInfo aci{};
    aci.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    if (dedicated) aci.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    return createImage(ici, aci);
}

void Allocator::destroy(Image& i) const {
    if (i.image) vmaDestroyImage(handle, static_cast<VkImage>(i.image), i.allocation);
    i = {};
}

// ---------------- Прочее ----------------
bool Allocator::flush(VmaAllocation a, vk::DeviceSize offset, vk::DeviceSize size) const {
    return okVk(vmaFlushAllocation(handle, a, offset, size), "vmaFlushAllocation");
}

bool Allocator::invalidate(VmaAllocation a, vk::DeviceSize offset, vk::DeviceSize size) const {
    return okVk(vmaInvalidateAllocation(handle, a, offset, size), "vmaInvalidateAllocation");
}

std::string Allocator::statsString(bool detailed) const {
    char* s = nullptr;
    vmaBuildStatsString(handle, &s, detailed ? VK_TRUE : VK_FALSE);
    std::string out = s ? s : "";
    if (s) vmaFreeStatsString(handle, s);
    return out;
}

} // namespace vkctx
