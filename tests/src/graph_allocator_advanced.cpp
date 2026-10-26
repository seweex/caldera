
#include <gtest/gtest.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include <caldera/structs.hpp>
#include <detail/graph_allocator.hpp>

class GraphAllocatorAdvancedTest;

extern "C" {
VkResult vmaAllocateMemory([[maybe_unused]] VmaAllocator allocator,
                           [[maybe_unused]] VkMemoryRequirements const *pVkMemoryRequirements,
                           [[maybe_unused]] VmaAllocationCreateInfo const *pCreateInfo, VmaAllocation *pAllocation,
                           [[maybe_unused]] VmaAllocationInfo *pAllocationInfo);

VkResult vmaAllocateMemoryForImage([[maybe_unused]] VmaAllocator allocator, VkImage image,
                                   [[maybe_unused]] VmaAllocationCreateInfo const *pCreateInfo,
                                   VmaAllocation *pAllocation, [[maybe_unused]] VmaAllocationInfo *pAllocationInfo);

VkResult vmaAllocateMemoryForBuffer([[maybe_unused]] VmaAllocator allocator, VkBuffer buffer,
                                    [[maybe_unused]] VmaAllocationCreateInfo const *pCreateInfo,
                                    VmaAllocation *pAllocation, [[maybe_unused]] VmaAllocationInfo *pAllocationInfo);

void vmaFreeMemory([[maybe_unused]] VmaAllocator allocator, [[maybe_unused]] VmaAllocation allocation);

VkResult vmaBindImageMemory2([[maybe_unused]] VmaAllocator allocator, [[maybe_unused]] VmaAllocation allocation,
                             [[maybe_unused]] VkDeviceSize allocationLocalOffset, [[maybe_unused]] VkImage image,
                             [[maybe_unused]] void const *pNext);

VkResult vmaBindBufferMemory2([[maybe_unused]] VmaAllocator allocator, [[maybe_unused]] VmaAllocation allocation,
                              [[maybe_unused]] VkDeviceSize allocationLocalOffset, [[maybe_unused]] VkBuffer buffer,
                              [[maybe_unused]] void const *pNext);

void vmaGetAllocationInfo([[maybe_unused]] VmaAllocator allocator, [[maybe_unused]] VmaAllocation allocation,
                          VmaAllocationInfo *pAllocationInfo);
}

class GraphAllocatorAdvancedTest : public ::testing::Test {
    template <class RequirementsTy>
    static void mock_vk_get_requirements_2([[maybe_unused]] VkDevice, [[maybe_unused]] RequirementsTy const *,
                                           VkMemoryRequirements2 *pMemoryRequirements) {
        pMemoryRequirements->memoryRequirements.size = requested_size;
        pMemoryRequirements->memoryRequirements.alignment = requested_alignment;
        pMemoryRequirements->memoryRequirements.memoryTypeBits = requested_memory_bits;

        if (pMemoryRequirements->pNext) {
            auto const next = static_cast<VkMemoryDedicatedRequirements *>(pMemoryRequirements->pNext);
            if (next->sType == VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS) {
                next->prefersDedicatedAllocation = requested_prefers_dedicated_allocation;
                next->requiresDedicatedAllocation = requested_requires_dedicated_allocation;
            }
        }
    }

    template <class InfoTy, class ResourceTy>
    static VkResult mock_vk_create_resource([[maybe_unused]] VkDevice, [[maybe_unused]] InfoTy const *pCreateInfo,
                                            [[maybe_unused]] VkAllocationCallbacks const *, ResourceTy *resource) {
        if constexpr (std::same_as<ResourceTy, VkImage>) {
            if (failAtImageCreationIndex && *failAtImageCreationIndex == createdImageCount)
                return VK_ERROR_OUT_OF_DEVICE_MEMORY;

            ++createdImageCount;
            *resource = reinterpret_cast<ResourceTy>(static_cast<uintptr_t>(0x2000 + createdImageCount));
        } else {
            if (failAtBufferCreationIndex && *failAtBufferCreationIndex == createdBufferCount)
                return VK_ERROR_OUT_OF_DEVICE_MEMORY;

            ++createdBufferCount;
            *resource = reinterpret_cast<ResourceTy>(static_cast<uintptr_t>(0x3000 + createdBufferCount));
        }

        return VK_SUCCESS;
    }

    template <class ResourceTy>
    static void mock_vk_destroy_resource([[maybe_unused]] VkDevice, [[maybe_unused]] ResourceTy,
                                         [[maybe_unused]] VkAllocationCallbacks const *) {
        if constexpr (std::same_as<ResourceTy, VkImage>)
            ++destroyedImageCount;
        else
            ++destroyedBufferCount;
    }

  public:
    static VkResult mock_vma_allocate_memory(VmaAllocation *pAllocation) {
        if (failAtVmaAllocIndex && *failAtVmaAllocIndex == vmaAllocatedCount)
            return VK_ERROR_OUT_OF_DEVICE_MEMORY;

        ++vmaAllocatedCount;
        *pAllocation = reinterpret_cast<VmaAllocation>(static_cast<uintptr_t>(0x4000 + vmaAllocatedCount));
        return VK_SUCCESS;
    }

    static VkResult mock_vma_allocate_for_image(VkImage, VmaAllocation *pAllocation) {
        ++vmaDedicatedImageAllocCount;
        return mock_vma_allocate_memory(pAllocation);
    }

    static VkResult mock_vma_allocate_for_buffer(VkBuffer, VmaAllocation *pAllocation) {
        ++vmaDedicatedBufferAllocCount;
        return mock_vma_allocate_memory(pAllocation);
    }

    static void mock_vma_free_memory() { ++vmaFreedCount; }

    static VkResult mock_vma_bind_image() {
        if (failAtVmaBindIndex && *failAtVmaBindIndex == vmaBoundImageCount + vmaBoundBufferCount)
            return VK_ERROR_INITIALIZATION_FAILED;

        ++vmaBoundImageCount;
        return VK_SUCCESS;
    }

    static VkResult mock_vma_bind_buffer() {
        if (failAtVmaBindIndex && *failAtVmaBindIndex == vmaBoundImageCount + vmaBoundBufferCount)
            return VK_ERROR_INITIALIZATION_FAILED;

        ++vmaBoundBufferCount;
        return VK_SUCCESS;
    }

    static void mock_vma_get_allocation_info(VmaAllocationInfo *pAllocationInfo) {
        pAllocationInfo->deviceMemory = dummyDeviceMemory;
        pAllocationInfo->pMappedData = dummyMappedData;
    }

  protected:
    void SetUp() override {
        vk::detail::defaultDispatchLoaderDynamic.vkGetImageMemoryRequirements2 =
            &mock_vk_get_requirements_2<VkImageMemoryRequirementsInfo2>;

        vk::detail::defaultDispatchLoaderDynamic.vkGetBufferMemoryRequirements2 =
            &mock_vk_get_requirements_2<VkBufferMemoryRequirementsInfo2>;

        vk::detail::defaultDispatchLoaderDynamic.vkCreateImage = &mock_vk_create_resource<VkImageCreateInfo, VkImage>;

        vk::detail::defaultDispatchLoaderDynamic.vkCreateBuffer =
            &mock_vk_create_resource<VkBufferCreateInfo, VkBuffer>;

        vk::detail::defaultDispatchLoaderDynamic.vkDestroyImage = &mock_vk_destroy_resource<VkImage>;

        vk::detail::defaultDispatchLoaderDynamic.vkDestroyBuffer = &mock_vk_destroy_resource<VkBuffer>;

        createdImageCount = 0;
        createdBufferCount = 0;

        destroyedImageCount = 0;
        destroyedBufferCount = 0;

        vmaAllocatedCount = 0;
        vmaFreedCount = 0;

        vmaDedicatedImageAllocCount = 0;
        vmaDedicatedBufferAllocCount = 0;

        vmaBoundImageCount = 0;
        vmaBoundBufferCount = 0;

        requested_prefers_dedicated_allocation = VK_FALSE;
        requested_requires_dedicated_allocation = VK_FALSE;

        failAtImageCreationIndex = std::nullopt;
        failAtBufferCreationIndex = std::nullopt;
        failAtVmaAllocIndex = std::nullopt;
        failAtVmaBindIndex = std::nullopt;
    }

    static constexpr vk::DeviceSize requested_size = 65535;
    static constexpr vk::DeviceSize requested_alignment = 1024;
    static constexpr vk::DeviceSize requested_memory_bits = 0b1010101010;

    static inline VkBool32 requested_prefers_dedicated_allocation = VK_FALSE;
    static inline VkBool32 requested_requires_dedicated_allocation = VK_FALSE;

    static inline const vk::Device dummyDevice = reinterpret_cast<VkDevice>(static_cast<uintptr_t>(0x1111));
    static inline const VmaAllocator dummyAllocator = reinterpret_cast<VmaAllocator>(static_cast<uintptr_t>(0x4444));
    static inline const VkDeviceMemory dummyDeviceMemory =
        reinterpret_cast<VkDeviceMemory>(static_cast<uintptr_t>(0x7777));
    static inline void *const dummyMappedData = reinterpret_cast<void *>(static_cast<uintptr_t>(0x8888));

    static constexpr caldera::FamilyNumbers dummyFamilies = {.graphics = 0, .transfer = 1, .compute = 2};

    static inline uint32_t createdImageCount = 0;
    static inline uint32_t createdBufferCount = 0;

    static inline uint32_t destroyedImageCount = 0;
    static inline uint32_t destroyedBufferCount = 0;

    static inline uint32_t vmaAllocatedCount = 0;
    static inline uint32_t vmaFreedCount = 0;

    static inline uint32_t vmaDedicatedImageAllocCount = 0;
    static inline uint32_t vmaDedicatedBufferAllocCount = 0;

    static inline uint32_t vmaBoundImageCount = 0;
    static inline uint32_t vmaBoundBufferCount = 0;

    static inline std::optional<uint32_t> failAtImageCreationIndex = std::nullopt;
    static inline std::optional<uint32_t> failAtBufferCreationIndex = std::nullopt;
    static inline std::optional<uint32_t> failAtVmaAllocIndex = std::nullopt;
    static inline std::optional<uint32_t> failAtVmaBindIndex = std::nullopt;
};

extern "C" {
VkResult vmaAllocateMemory([[maybe_unused]] VmaAllocator allocator,
                           [[maybe_unused]] VkMemoryRequirements const *pVkMemoryRequirements,
                           [[maybe_unused]] VmaAllocationCreateInfo const *pCreateInfo, VmaAllocation *pAllocation,
                           [[maybe_unused]] VmaAllocationInfo *pAllocationInfo) {
    return GraphAllocatorAdvancedTest::mock_vma_allocate_memory(pAllocation);
}

VkResult vmaAllocateMemoryForImage([[maybe_unused]] VmaAllocator allocator, VkImage image,
                                   [[maybe_unused]] VmaAllocationCreateInfo const *pCreateInfo,
                                   VmaAllocation *pAllocation, [[maybe_unused]] VmaAllocationInfo *pAllocationInfo) {
    return GraphAllocatorAdvancedTest::mock_vma_allocate_for_image(image, pAllocation);
}

VkResult vmaAllocateMemoryForBuffer([[maybe_unused]] VmaAllocator allocator, VkBuffer buffer,
                                    [[maybe_unused]] VmaAllocationCreateInfo const *pCreateInfo,
                                    VmaAllocation *pAllocation, [[maybe_unused]] VmaAllocationInfo *pAllocationInfo) {
    return GraphAllocatorAdvancedTest::mock_vma_allocate_for_buffer(buffer, pAllocation);
}

void vmaFreeMemory([[maybe_unused]] VmaAllocator allocator, [[maybe_unused]] VmaAllocation allocation) {
    GraphAllocatorAdvancedTest::mock_vma_free_memory();
}

VkResult vmaBindImageMemory2([[maybe_unused]] VmaAllocator allocator, [[maybe_unused]] VmaAllocation allocation,
                             [[maybe_unused]] VkDeviceSize allocationLocalOffset, [[maybe_unused]] VkImage image,
                             [[maybe_unused]] void const *pNext) {
    return GraphAllocatorAdvancedTest::mock_vma_bind_image();
}

VkResult vmaBindBufferMemory2([[maybe_unused]] VmaAllocator allocator, [[maybe_unused]] VmaAllocation allocation,
                              [[maybe_unused]] VkDeviceSize allocationLocalOffset, [[maybe_unused]] VkBuffer buffer,
                              [[maybe_unused]] void const *pNext) {
    return GraphAllocatorAdvancedTest::mock_vma_bind_buffer();
}

void vmaGetAllocationInfo([[maybe_unused]] VmaAllocator allocator, [[maybe_unused]] VmaAllocation allocation,
                          VmaAllocationInfo *pAllocationInfo) {
    GraphAllocatorAdvancedTest::mock_vma_get_allocation_info(pAllocationInfo);
}
}

namespace cd = caldera::detail;

TEST_F(GraphAllocatorAdvancedTest, DescribeAllocationsPrefersDedicatedMemory) {
    requested_prefers_dedicated_allocation = VK_TRUE;
    requested_requires_dedicated_allocation = VK_FALSE;

    caldera::ImageID constexpr dummyId = {.isTransient = 1, .index = 0};

    cd::ImageTrackingTable const table = {{dummyId, caldera::detail::ImageTrackedInfo{}}};

    vk::Image const dummyImage = reinterpret_cast<VkImage>(static_cast<uintptr_t>(0x2222));
    std::vector const allocated = {dummyImage};

    auto const result = cd::describe_allocations<caldera::ImageTag>(dummyDevice, table, allocated);

    ASSERT_EQ(result.descriptionList.size(), 1);
    EXPECT_TRUE(result.descriptionList[0].createFlags & VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
    EXPECT_TRUE(std::holds_alternative<vk::Image>(result.descriptionList[0].dedicatedTarget));
    EXPECT_EQ(std::get<vk::Image>(result.descriptionList[0].dedicatedTarget), dummyImage);
}

TEST_F(GraphAllocatorAdvancedTest, DescribeAllocationsNoDedicatedMemory) {
    requested_prefers_dedicated_allocation = VK_FALSE;
    requested_requires_dedicated_allocation = VK_FALSE;

    caldera::BufferID constexpr dummyId = {.isTransient = 1, .index = 0};

    cd::BufferTrackingTable const table = {{dummyId, caldera::detail::BufferTrackedInfo{}}};

    vk::Buffer const dummyBuffer = reinterpret_cast<VkBuffer>(static_cast<uintptr_t>(0x3333));
    std::vector const allocated = {dummyBuffer};

    auto const result = cd::describe_allocations<caldera::BufferTag>(dummyDevice, table, allocated);

    ASSERT_EQ(result.descriptionList.size(), 1);
    EXPECT_FALSE(result.descriptionList[0].createFlags & VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
    EXPECT_TRUE(std::holds_alternative<std::monostate>(result.descriptionList[0].dedicatedTarget));
}

TEST_F(GraphAllocatorAdvancedTest, AllocateSuccessAndDeallocate) {
    caldera::GraphIR const ir{.transientImages = {caldera::ImageDescription{}, caldera::ImageDescription{}},
                              .transientBuffers = {caldera::BufferDescription{}},
                              .passes = {},
                              .imageVersions = {},
                              .bufferVersions = {}};

    cd::ImageTrackingTable const imageTracking = {{caldera::ImageID{1, 0}, cd::ImageTrackedInfo{}},
                                                  {caldera::ImageID{1, 1}, cd::ImageTrackedInfo{}}};

    cd::BufferTrackingTable const bufferTracking = {{caldera::BufferID{1, 0}, cd::BufferTrackedInfo{}}};

    cd::GraphAllocator allocator(dummyDevice, dummyAllocator, dummyFamilies);

    auto result = allocator.allocate(ir, imageTracking, bufferTracking);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->transientImages.size(), 2);
    EXPECT_EQ(result->transientBuffers.size(), 1);
    EXPECT_EQ(result->imageHeaps.size(), 2);
    EXPECT_EQ(result->bufferHeaps.size(), 1);

    EXPECT_EQ(result->imageHeaps[0].memory, dummyDeviceMemory);
    EXPECT_EQ(result->imageHeaps[0].mapped, dummyMappedData);
    EXPECT_EQ(result->transientImages[0].heapIdx, 0);
    EXPECT_EQ(result->transientImages[1].heapIdx, 1);

    EXPECT_EQ(createdImageCount, 2);
    EXPECT_EQ(createdBufferCount, 1);
    EXPECT_EQ(vmaAllocatedCount, 3);
    EXPECT_EQ(vmaBoundImageCount, 2);
    EXPECT_EQ(vmaBoundBufferCount, 1);

    allocator.deallocate(*result);

    EXPECT_EQ(destroyedImageCount, 2);
    EXPECT_EQ(destroyedBufferCount, 1);
    EXPECT_EQ(vmaFreedCount, 3);
    EXPECT_TRUE(result->transientImages.empty());
    EXPECT_TRUE(result->transientBuffers.empty());
    EXPECT_TRUE(result->imageHeaps.empty());
    EXPECT_TRUE(result->bufferHeaps.empty());
}

TEST_F(GraphAllocatorAdvancedTest, AllocateRollbackOnBufferFailure) {
    caldera::GraphIR const ir{.transientImages = {caldera::ImageDescription{}},
                              .transientBuffers = {caldera::BufferDescription{}},
                              .passes = {},
                              .imageVersions = {},
                              .bufferVersions = {}};

    cd::ImageTrackingTable const imageTracking = {{caldera::ImageID{1, 0}, cd::ImageTrackedInfo{}}};

    cd::BufferTrackingTable const bufferTracking = {{caldera::BufferID{1, 0}, cd::BufferTrackedInfo{}}};

    failAtBufferCreationIndex = 0;

    cd::GraphAllocator allocator(dummyDevice, dummyAllocator, dummyFamilies);

    auto result = allocator.allocate(ir, imageTracking, bufferTracking);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), vk::Result::eErrorOutOfDeviceMemory);

    EXPECT_EQ(createdImageCount, 1);
    EXPECT_EQ(destroyedImageCount, 1);
    EXPECT_EQ(vmaAllocatedCount, 1);
    EXPECT_EQ(vmaFreedCount, 1);
}

TEST_F(GraphAllocatorAdvancedTest, AllocateRollbackOnVmaAllocationFailure) {
    caldera::GraphIR const ir{.transientImages = {caldera::ImageDescription{}},
                              .transientBuffers = {},
                              .passes = {},
                              .imageVersions = {},
                              .bufferVersions = {}};

    cd::ImageTrackingTable const imageTracking = {{caldera::ImageID{1, 0}, cd::ImageTrackedInfo{}}};

    cd::BufferTrackingTable const bufferTracking;

    failAtVmaAllocIndex = 0;

    cd::GraphAllocator allocator(dummyDevice, dummyAllocator, dummyFamilies);

    auto result = allocator.allocate(ir, imageTracking, bufferTracking);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), vk::Result::eErrorOutOfDeviceMemory);

    EXPECT_EQ(createdImageCount, 1);
    EXPECT_EQ(destroyedImageCount, 1);
    EXPECT_EQ(vmaAllocatedCount, 0);
    EXPECT_EQ(vmaFreedCount, 0);
}

TEST_F(GraphAllocatorAdvancedTest, AllocateRollbackOnVmaBindFailure) {
    caldera::GraphIR const ir{.transientImages = {caldera::ImageDescription{}},
                              .transientBuffers = {},
                              .passes = {},
                              .imageVersions = {},
                              .bufferVersions = {}};

    cd::ImageTrackingTable const imageTracking = {{caldera::ImageID{1, 0}, cd::ImageTrackedInfo{}}};

    cd::BufferTrackingTable const bufferTracking;

    failAtVmaBindIndex = 0;

    cd::GraphAllocator allocator(dummyDevice, dummyAllocator, dummyFamilies);

    auto result = allocator.allocate(ir, imageTracking, bufferTracking);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), vk::Result::eErrorInitializationFailed);

    EXPECT_EQ(createdImageCount, 1);
    EXPECT_EQ(destroyedImageCount, 1);
    EXPECT_EQ(vmaAllocatedCount, 1);
    EXPECT_EQ(vmaFreedCount, 1);
}