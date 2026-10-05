#include "vk_context.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <set>

namespace vkctx {

namespace {

// ---------- nothrow-хелперы ----------
bool ok(vk::Result r, const char* what) {
    if (r == vk::Result::eSuccess) return true;
    std::fprintf(stderr, "[vk] %s failed: %s\n", what, vk::to_string(r).c_str());
    return false;
}

template <class T>
std::optional<T> take(vk::ResultValue<T>&& rv, const char* what) {
    if (!ok(rv.result, what)) return std::nullopt;
    return std::move(rv.value);
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                             VkDebugUtilsMessageTypeFlagsEXT,
                                             const VkDebugUtilsMessengerCallbackDataEXT* data,
                                             void*) {
    const char* tag = (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)     ? "ERROR"
                      : (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) ? "WARN"
                                                                                     : "INFO";
    std::fprintf(stderr, "[vk-validation][%s] %s\n", tag, data->pMessage);
    return VK_FALSE;
}

struct Families {
    uint32_t gfx = UINT32_MAX;
    uint32_t compute = UINT32_MAX;
    uint32_t transfer = UINT32_MAX;
    uint32_t present = UINT32_MAX;
};

Families findFamilies(vk::PhysicalDevice pd, vk::SurfaceKHR surface) {
    Families f;
    auto props = pd.getQueueFamilyProperties();
    const uint32_t n = static_cast<uint32_t>(props.size());

    for (uint32_t i = 0; i < n; ++i) {
        if (props[i].queueFlags & vk::QueueFlagBits::eGraphics) { f.gfx = i; break; }
    }
    // Выделенный compute: compute без graphics
    for (uint32_t i = 0; i < n; ++i) {
        auto fl = props[i].queueFlags;
        if ((fl & vk::QueueFlagBits::eCompute) && !(fl & vk::QueueFlagBits::eGraphics)) { f.compute = i; break; }
    }
    if (f.compute == UINT32_MAX) f.compute = f.gfx;
    // Выделенный transfer: transfer без graphics и compute
    for (uint32_t i = 0; i < n; ++i) {
        auto fl = props[i].queueFlags;
        if ((fl & vk::QueueFlagBits::eTransfer) &&
            !(fl & (vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute))) { f.transfer = i; break; }
    }
    if (f.transfer == UINT32_MAX) f.transfer = f.compute != UINT32_MAX ? f.compute : f.gfx;

    if (surface) {
        auto supports = [&](uint32_t i) {
            auto r = pd.getSurfaceSupportKHR(i, surface);
            return r.result == vk::Result::eSuccess && r.value == VK_TRUE;
        };
        if (f.gfx != UINT32_MAX && supports(f.gfx)) {
            f.present = f.gfx;
        } else {
            for (uint32_t i = 0; i < n; ++i) if (supports(i)) { f.present = i; break; }
        }
    }
    return f;
}

// Сырой pNext-чейн: структуры 1.3 подцепляем только если устройство их знает.
struct FeatureQuery {
    vk::PhysicalDeviceFeatures2 f2;
    vk::PhysicalDeviceVulkan12Features f12;
    vk::PhysicalDeviceVulkan13Features f13;

    FeatureQuery(vk::PhysicalDevice pd, uint32_t api) {
        f2.pNext = &f12;
        if (api >= VK_API_VERSION_1_3) f12.pNext = &f13;
        pd.getFeatures2(&f2);
    }
};

} // namespace

// =====================================================================
bool VulkanContext::init(const ContextConfig& cfg) {
    if (!createInstance(cfg)) return false;
    if (cfg.createSurface) {
        surface = cfg.createSurface(instance);
        if (!surface) { std::fprintf(stderr, "[vk] createSurface returned null\n"); return false; }
    }
    return pickPhysicalDevice(cfg) && createDevice(cfg) && createQueuePools();
}

bool VulkanContext::createInstance(const ContextConfig& cfg) {
    auto loaderVer = take(vk::enumerateInstanceVersion(), "enumerateInstanceVersion");
    if (!loaderVer) return false;
    instanceApiVersion = std::min<uint32_t>(*loaderVer, VK_API_VERSION_1_3);
    if (instanceApiVersion < cfg.minApiVersion) {
        std::fprintf(stderr, "[vk] loader API too old\n");
        return false;
    }

    std::vector<const char*> layers;
    std::vector<const char*> exts = cfg.instanceExtensions;
    bool useDebug = false;

    if (cfg.enableValidation) {
        constexpr const char* kValidation = "VK_LAYER_KHRONOS_validation";
        auto lp = take(vk::enumerateInstanceLayerProperties(), "enumerateInstanceLayerProperties");
        bool haveLayer = lp && std::ranges::any_of(*lp, [&](const vk::LayerProperties& l) {
            return std::strcmp(l.layerName.data(), kValidation) == 0;
        });
        auto ep = take(vk::enumerateInstanceExtensionProperties(), "enumerateInstanceExtensionProperties");
        bool haveDbg = ep && std::ranges::any_of(*ep, [&](const vk::ExtensionProperties& e) {
            return std::strcmp(e.extensionName.data(), VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0;
        });
        if (haveLayer && haveDbg) {
            layers.push_back(kValidation);
            exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            useDebug = true;
        } else {
            std::fprintf(stderr, "[vk] validation layer/debug utils not available, continuing without\n");
        }
    }

    vk::ApplicationInfo app;
    app.setPApplicationName(cfg.appName.c_str())
        .setApplicationVersion(1)
        .setPEngineName("RenderGraph")
        .setEngineVersion(1)
        .setApiVersion(instanceApiVersion);

    vk::InstanceCreateInfo ici;
    ici.setPApplicationInfo(&app).setPEnabledLayerNames(layers).setPEnabledExtensionNames(exts);

    auto inst = take(vk::createInstance(ici), "createInstance");
    if (!inst) return false;
    instance = *inst;

    if (useDebug) {
        auto pfnCreate = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(static_cast<VkInstance>(instance), "vkCreateDebugUtilsMessengerEXT"));
        pfnDestroyMessenger_ = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(static_cast<VkInstance>(instance), "vkDestroyDebugUtilsMessengerEXT"));

        vk::DebugUtilsMessengerCreateInfoEXT mci;
        mci.setMessageSeverity(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                               vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
            .setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                            vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)
            .setPfnUserCallback(&debugCallback);

        VkDebugUtilsMessengerEXT raw = VK_NULL_HANDLE;
        if (pfnCreate && pfnDestroyMessenger_ &&
            ok(static_cast<vk::Result>(pfnCreate(static_cast<VkInstance>(instance),
                   &static_cast<const VkDebugUtilsMessengerCreateInfoEXT&>(mci), nullptr, &raw)),
               "vkCreateDebugUtilsMessengerEXT"))
            debugMessenger = vk::DebugUtilsMessengerEXT(raw);
    }
    return true;
}

bool VulkanContext::pickPhysicalDevice(const ContextConfig& cfg) {
    auto devs = take(instance.enumeratePhysicalDevices(), "enumeratePhysicalDevices");
    if (!devs || devs->empty()) { std::fprintf(stderr, "[vk] no physical devices\n"); return false; }

    int bestScore = -1;
    for (vk::PhysicalDevice pd : *devs) {
        const auto props = pd.getProperties();
        const uint32_t api = std::min(props.apiVersion, instanceApiVersion);
        if (api < cfg.minApiVersion) continue;

        auto exts = take(pd.enumerateDeviceExtensionProperties(), "enumerateDeviceExtensionProperties");
        if (!exts) continue;
        bool allExts = std::ranges::all_of(cfg.deviceExtensions, [&](const char* want) {
            return std::ranges::any_of(*exts, [&](const vk::ExtensionProperties& e) {
                return std::strcmp(e.extensionName.data(), want) == 0;
            });
        });
        if (!allExts) continue;

        FeatureQuery fq(pd, api);
        if (!fq.f12.timelineSemaphore) continue;

        Families fam = findFamilies(pd, surface);
        if (fam.gfx == UINT32_MAX) continue;
        if (surface && fam.present == UINT32_MAX) continue;

        int score = 0;
        if (props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) score += cfg.preferDiscreteGpu ? 1000 : 0;
        else if (props.deviceType == vk::PhysicalDeviceType::eIntegratedGpu) score += 100;
        score += (api >= VK_API_VERSION_1_3) ? 10 : 0;

        if (score > bestScore) {
            bestScore = score;
            physicalDevice = pd;
            deviceApiVersion = api;
        }
    }
    if (!physicalDevice) { std::fprintf(stderr, "[vk] no suitable physical device\n"); return false; }

    properties = physicalDevice.getProperties();
    memoryProperties = physicalDevice.getMemoryProperties();
    std::fprintf(stderr, "[vk] using GPU: %s (API %u.%u)\n", properties.deviceName.data(),
                 VK_API_VERSION_MAJOR(deviceApiVersion), VK_API_VERSION_MINOR(deviceApiVersion));
    return true;
}

bool VulkanContext::createDevice(const ContextConfig& cfg) {
    const Families fam = findFamilies(physicalDevice, surface);
    graphics.family = fam.gfx;
    compute.family = fam.compute;
    transfer.family = fam.transfer;
    if (surface) present.family = fam.present;

    // По одной очереди на уникальную семью
    std::set<uint32_t> uniqueFamilies{graphics.family, compute.family, transfer.family};
    if (present.valid()) uniqueFamilies.insert(present.family);

    const float priority = 1.0f;
    std::vector<vk::DeviceQueueCreateInfo> qcis;
    for (uint32_t f : uniqueFamilies) {
        vk::DeviceQueueCreateInfo q;
        q.setQueueFamilyIndex(f).setQueueCount(1).setPQueuePriorities(&priority);
        qcis.push_back(q);
    }

    // Фичи: включаем только то, что реально поддерживается
    FeatureQuery sup(physicalDevice, deviceApiVersion);
    vk::PhysicalDeviceFeatures2 en2;
    vk::PhysicalDeviceVulkan12Features en12;
    vk::PhysicalDeviceVulkan13Features en13;
    en2.pNext = &en12;
    if (deviceApiVersion >= VK_API_VERSION_1_3) en12.pNext = &en13;

    en2.features.samplerAnisotropy = sup.f2.features.samplerAnisotropy;
    en12.timelineSemaphore = VK_TRUE;
    en12.bufferDeviceAddress = sup.f12.bufferDeviceAddress;
    en12.descriptorIndexing = sup.f12.descriptorIndexing;
    if (deviceApiVersion >= VK_API_VERSION_1_3) {
        en13.synchronization2 = sup.f13.synchronization2;
        en13.dynamicRendering = sup.f13.dynamicRendering;
    }
    enabled = {
        .timelineSemaphore = true,
        .bufferDeviceAddress = en12.bufferDeviceAddress == VK_TRUE,
        .descriptorIndexing = en12.descriptorIndexing == VK_TRUE,
        .synchronization2 = en13.synchronization2 == VK_TRUE,
        .dynamicRendering = en13.dynamicRendering == VK_TRUE,
    };

    vk::DeviceCreateInfo dci;
    dci.setPNext(&en2) // features2 в pNext => pEnabledFeatures должен быть nullptr
        .setQueueCreateInfos(qcis)
        .setPEnabledExtensionNames(cfg.deviceExtensions);

    auto dev = take(physicalDevice.createDevice(dci), "createDevice");
    if (!dev) return false;
    device = *dev;

    auto fetch = [&](QueueInfo& qi) {
        if (!qi.valid()) return;
        qi.index = 0;
        qi.queue = device.getQueue(qi.family, 0);
    };
    fetch(graphics);
    fetch(compute);
    fetch(transfer);
    fetch(present);
    return true;
}

bool VulkanContext::createQueuePools() {
    for (QueueInfo* q : {&graphics, &compute, &transfer, &present}) {
        if (!q->valid()) continue;
        auto p = createCommandPool(q->family);
        if (!p) return false;
        q->pool = *p;
    }
    return true;
}

void VulkanContext::shutdown() {
    if (device) {
        (void)device.waitIdle();
        for (QueueInfo* q : {&graphics, &compute, &transfer, &present}) {
            if (q->pool) device.destroyCommandPool(q->pool);
            *q = {};
        }
        device.destroy();
        device = vk::Device{};
    }
    if (instance) {
        if (surface) instance.destroySurfaceKHR(surface);
        if (debugMessenger && pfnDestroyMessenger_)
            pfnDestroyMessenger_(static_cast<VkInstance>(instance),
                                 static_cast<VkDebugUtilsMessengerEXT>(debugMessenger), nullptr);
        instance.destroy();
        instance = vk::Instance{};
    }
    surface = vk::SurfaceKHR{};
    debugMessenger = vk::DebugUtilsMessengerEXT{};
    physicalDevice = vk::PhysicalDevice{};
}

// =====================================================================
// Хелперы
std::optional<vk::CommandPool> VulkanContext::createCommandPool(uint32_t family,
                                                                vk::CommandPoolCreateFlags flags) const {
    vk::CommandPoolCreateInfo ci;
    ci.setQueueFamilyIndex(family).setFlags(flags);
    return take(device.createCommandPool(ci), "createCommandPool");
}

std::vector<vk::CommandBuffer> VulkanContext::allocateCommandBuffers(vk::CommandPool pool,
                                                                     vk::CommandBufferLevel level,
                                                                     uint32_t count) const {
    vk::CommandBufferAllocateInfo ai;
    ai.setCommandPool(pool).setLevel(level).setCommandBufferCount(count);
    return take(device.allocateCommandBuffers(ai), "allocateCommandBuffers").value_or(std::vector<vk::CommandBuffer>{});
}

std::optional<vk::Semaphore> VulkanContext::createBinarySemaphore() const {
    return take(device.createSemaphore(vk::SemaphoreCreateInfo{}), "createSemaphore");
}

std::optional<vk::Fence> VulkanContext::createFence(bool signaled) const {
    vk::FenceCreateInfo ci;
    if (signaled) ci.setFlags(vk::FenceCreateFlagBits::eSignaled);
    return take(device.createFence(ci), "createFence");
}

std::optional<TimelineSemaphore> VulkanContext::createTimelineSemaphore(uint64_t initialValue) const {
    vk::SemaphoreTypeCreateInfo type;
    type.setSemaphoreType(vk::SemaphoreType::eTimeline).setInitialValue(initialValue);
    vk::SemaphoreCreateInfo ci;
    ci.setPNext(&type);
    auto s = take(device.createSemaphore(ci), "createSemaphore(timeline)");
    if (!s) return std::nullopt;
    return TimelineSemaphore{.handle = *s, .lastSignaled = initialValue};
}

void VulkanContext::destroyTimelineSemaphore(TimelineSemaphore& s) const {
    if (s.handle) device.destroySemaphore(s.handle);
    s = {};
}

bool VulkanContext::waitTimeline(const TimelineSemaphore& s, uint64_t value, uint64_t timeoutNs) const {
    vk::SemaphoreWaitInfo wi;
    wi.setSemaphores(s.handle).setValues(value);
    const vk::Result r = device.waitSemaphores(wi, timeoutNs);
    if (r == vk::Result::eTimeout) return false;
    return ok(r, "waitSemaphores");
}

bool VulkanContext::signalTimeline(const TimelineSemaphore& s, uint64_t value) const {
    vk::SemaphoreSignalInfo si;
    si.setSemaphore(s.handle).setValue(value);
    return ok(device.signalSemaphore(si), "signalSemaphore");
}

std::optional<uint64_t> VulkanContext::timelineValue(const TimelineSemaphore& s) const {
    return take(device.getSemaphoreCounterValue(s.handle), "getSemaphoreCounterValue");
}

bool VulkanContext::submit(vk::Queue queue,
                           std::span<const vk::CommandBuffer> cmds,
                           std::span<const TimelinePoint> waits,
                           std::span<const TimelinePoint> signals,
                           vk::Fence fence) const {
    std::vector<vk::Semaphore> waitSems, signalSems;
    std::vector<vk::PipelineStageFlags> stages;
    std::vector<uint64_t> waitVals, signalVals;
    for (const auto& w : waits) {
        waitSems.push_back(w.sem);
        waitVals.push_back(w.value);
        stages.push_back(w.stage);
    }
    for (const auto& s : signals) {
        signalSems.push_back(s.sem);
        signalVals.push_back(s.value);
    }

    vk::TimelineSemaphoreSubmitInfo tl;
    tl.setWaitSemaphoreValues(waitVals).setSignalSemaphoreValues(signalVals);

    vk::SubmitInfo si;
    si.setPNext(&tl)
        .setCommandBufferCount(static_cast<uint32_t>(cmds.size()))
        .setPCommandBuffers(cmds.data())
        .setWaitSemaphores(waitSems)
        .setWaitDstStageMask(stages)
        .setSignalSemaphores(signalSems);
    return ok(queue.submit(si, fence), "queueSubmit");
}

std::optional<uint32_t> VulkanContext::findMemoryType(uint32_t typeBits, vk::MemoryPropertyFlags required) const {
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
        if ((typeBits & (1u << i)) &&
            (memoryProperties.memoryTypes[i].propertyFlags & required) == required)
            return i;
    }
    return std::nullopt;
}

} // namespace vkctx