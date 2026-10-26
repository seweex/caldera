
#include <gtest/gtest.h>
#include <vulkan/vulkan.hpp>

#include <caldera/structs.hpp>
#include <detail/graph_allocator.hpp>

class GraphAllocatorTest : public ::testing::Test {
    template <class RequirementsTy>
    static void mock_vk_get_requirements_2([[maybe_unused]] VkDevice, [[maybe_unused]] RequirementsTy const *,
                                           VkMemoryRequirements2 *pMemoryRequirements) {
        pMemoryRequirements->memoryRequirements.size = requested_size;
        pMemoryRequirements->memoryRequirements.alignment = requested_alignment;
        pMemoryRequirements->memoryRequirements.memoryTypeBits = requested_memory_bits;

        if (pMemoryRequirements->pNext) {
            auto const next = static_cast<VkMemoryDedicatedRequirements *>(pMemoryRequirements->pNext);
            if (next->sType == VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS)
                next->requiresDedicatedAllocation = requested_dedicated_allocation;
        }
    }

    template <class InfoTy, class ResourceTy>
    static VkResult mock_vk_create_resource([[maybe_unused]] VkDevice, [[maybe_unused]] InfoTy const *pCreateInfo,
                                            [[maybe_unused]] VkAllocationCallbacks const *, ResourceTy *resource) {
        uint32_t const currentIndex = std::same_as<ResourceTy, VkImage> ? createdImageCount : createdBufferCount;

        if (failAtCreationIndex && *failAtCreationIndex == currentIndex)
            return VK_ERROR_OUT_OF_DEVICE_MEMORY;

        if constexpr (std::same_as<ResourceTy, VkImage>)
            ++createdImageCount;
        else
            ++createdBufferCount;

        *resource = reinterpret_cast<ResourceTy>(static_cast<uintptr_t>(0x9999 + currentIndex));

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

        failAtCreationIndex = std::nullopt;
    }

    static constexpr vk::DeviceSize requested_size = 65535;
    static constexpr vk::DeviceSize requested_alignment = 1024;
    static constexpr vk::DeviceSize requested_memory_bits = 0b1010101010;
    static constexpr VkBool32 requested_dedicated_allocation = VK_TRUE;

    static inline const vk::Device dummyDevice = reinterpret_cast<VkDevice>(static_cast<uintptr_t>(0x1111));
    static inline const vk::Image dummyImage = reinterpret_cast<VkImage>(static_cast<uintptr_t>(0x2222));
    static inline const vk::Buffer dummyBuffer = reinterpret_cast<VkBuffer>(static_cast<uintptr_t>(0x3333));

    static constexpr caldera::FamilyNumbers dummyFamilies = {.graphics = 0, .transfer = 1, .compute = 2};

    static inline uint32_t createdImageCount = 0;
    static inline uint32_t createdBufferCount = 0;

    static inline uint32_t destroyedImageCount = 0;
    static inline uint32_t destroyedBufferCount = 0;

    static inline std::optional<uint32_t> failAtCreationIndex = std::nullopt;
};

namespace cd = caldera::detail;

TEST_F(GraphAllocatorTest, MappingPolicyChoice) {
    caldera::detail::BufferTrackedInfo constexpr frequentlyUsedWithMapping{.mappedAccessPercentage = 50};

    caldera::detail::BufferTrackedInfo constexpr rarelyUsedWithMapping{.mappedAccessPercentage = 20};

    EXPECT_EQ(caldera::MappingPolicy::unmappable, cd::choose_mapping_policy({.mappedAccessPercentage = 0}));
    EXPECT_NE(caldera::MappingPolicy::unmappable, cd::choose_mapping_policy({.mappedAccessPercentage = 1}));

    EXPECT_NE(caldera::MappingPolicy::dynamic_noncoherent, cd::choose_mapping_policy(frequentlyUsedWithMapping));
    EXPECT_EQ(caldera::MappingPolicy::dynamic_noncoherent, cd::choose_mapping_policy(rarelyUsedWithMapping));
}

TEST_F(GraphAllocatorTest, ImageMemoryDescription) {
    caldera::ImageID constexpr dummyId = {.isTransient = 1, .index = 0};

    cd::ImageTrackingTable const table = {{dummyId, caldera::detail::ImageTrackedInfo{}}};

    std::vector const allocated = {dummyImage};

    auto const result = cd::describe_allocations<caldera::ImageTag>(dummyDevice, table, allocated);

    EXPECT_TRUE(result.descriptionList.size() == 1);
    EXPECT_TRUE(result.resourceBindings[dummyId.index].heapIdx == 0);

    EXPECT_TRUE(result.descriptionList[0].size == requested_size);
    EXPECT_TRUE(result.descriptionList[0].alignment == requested_alignment);
    EXPECT_TRUE(result.descriptionList[0].memoryTypeBits == requested_memory_bits);

    EXPECT_TRUE(requested_dedicated_allocation ==
                static_cast<bool>(result.descriptionList[0].createFlags & VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT));
}

TEST_F(GraphAllocatorTest, BufferMemoryDescriptionBasic) {
    caldera::BufferID constexpr dummyId = {.isTransient = 1, .index = 0};

    cd::BufferTrackingTable const table = {{dummyId, caldera::detail::BufferTrackedInfo{}}};

    std::vector const allocated = {dummyBuffer};

    auto const result = cd::describe_allocations<caldera::BufferTag>(dummyDevice, table, allocated);

    EXPECT_TRUE(result.descriptionList.size() == 1);
    EXPECT_TRUE(result.resourceBindings[dummyId.index].heapIdx == 0);

    EXPECT_TRUE(result.descriptionList[0].size == requested_size);
    EXPECT_TRUE(result.descriptionList[0].alignment == requested_alignment);
    EXPECT_TRUE(result.descriptionList[0].memoryTypeBits == requested_memory_bits);

    EXPECT_TRUE(requested_dedicated_allocation ==
                static_cast<bool>(result.descriptionList[0].createFlags & VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT));
}

TEST_F(GraphAllocatorTest, BufferMemoryDescriptionFlagsForPersistentCoherentRandomAccess) {
    caldera::BufferID constexpr dummyId = {.isTransient = 1, .index = 0};

    cd::BufferTrackingTable const table = {{dummyId, cd::BufferTrackedInfo{.mappedAccessPercentage = 80,
                                                                           .hasRandomMappedAccess = true,
                                                                           .hasMappedReading = false}}};

    std::vector const allocated = {dummyBuffer};
    auto const result = cd::describe_allocations<caldera::BufferTag>(dummyDevice, table, allocated);

    ASSERT_EQ(result.descriptionList.size(), 1);
    auto const &desc = result.descriptionList[0];

    EXPECT_TRUE(desc.createFlags & VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
    EXPECT_FALSE(desc.createFlags & VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    EXPECT_TRUE(desc.createFlags & VMA_ALLOCATION_CREATE_MAPPED_BIT);
    EXPECT_TRUE(desc.requiredFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    EXPECT_FALSE(desc.preferredFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
}

TEST_F(GraphAllocatorTest, BufferMemoryDescriptionFlagsForPersistentNoncoherentCachedReading) {
    caldera::BufferID constexpr dummyId = {.isTransient = 1, .index = 0};

    cd::BufferTrackingTable const table = {{dummyId, cd::BufferTrackedInfo{.mappedAccessPercentage = 45,
                                                                           .hasRandomMappedAccess = false,
                                                                           .hasMappedReading = true}}};

    std::vector const allocated = {dummyBuffer};
    auto const result = cd::describe_allocations<caldera::BufferTag>(dummyDevice, table, allocated);

    ASSERT_EQ(result.descriptionList.size(), 1);
    auto const &desc = result.descriptionList[0];

    EXPECT_TRUE(desc.createFlags & VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
    EXPECT_TRUE(desc.createFlags & VMA_ALLOCATION_CREATE_MAPPED_BIT);
    EXPECT_FALSE(desc.requiredFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    EXPECT_TRUE(desc.preferredFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
}

TEST_F(GraphAllocatorTest, BufferMemoryDescriptionFlagsForDynamicSequentialWrite) {
    caldera::BufferID constexpr dummyId = {.isTransient = 1, .index = 0};

    cd::BufferTrackingTable const table = {{dummyId, cd::BufferTrackedInfo{.mappedAccessPercentage = 15,
                                                                           .hasRandomMappedAccess = false,
                                                                           .hasMappedReading = false}}};

    std::vector const allocated = {dummyBuffer};
    auto const result = cd::describe_allocations<caldera::BufferTag>(dummyDevice, table, allocated);

    ASSERT_EQ(result.descriptionList.size(), 1);
    auto const &desc = result.descriptionList[0];

    EXPECT_TRUE(desc.createFlags & VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    EXPECT_FALSE(desc.createFlags & VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
    EXPECT_FALSE(desc.createFlags & VMA_ALLOCATION_CREATE_MAPPED_BIT);
    EXPECT_FALSE(desc.requiredFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    EXPECT_FALSE(desc.preferredFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
}

TEST_F(GraphAllocatorTest, ImageLifecycle) {
    std::vector const descriptionList = {caldera::ImageDescription{}, caldera::ImageDescription{},
                                         caldera::ImageDescription{}, caldera::ImageDescription{}};

    cd::ImageTrackingTable const trackingTable = {{caldera::ImageID{1, 0}, cd::ImageTrackedInfo{}},
                                                  {caldera::ImageID{1, 1}, cd::ImageTrackedInfo{}},
                                                  {caldera::ImageID{1, 2}, cd::ImageTrackedInfo{}},
                                                  {caldera::ImageID{1, 3}, cd::ImageTrackedInfo{}}};

    auto const resources = cd::create_images(dummyDevice, dummyFamilies, descriptionList, trackingTable);
    EXPECT_TRUE(resources.has_value());
    EXPECT_TRUE(resources->size() == descriptionList.size());

    ASSERT_TRUE(createdImageCount != 0);
    EXPECT_TRUE(resources->size() == createdImageCount);

    cd::destroy_images(dummyDevice, *resources);
    EXPECT_TRUE(destroyedImageCount == createdImageCount);
}

TEST_F(GraphAllocatorTest, ImageCreationFailureRollback) {
    std::vector const descriptionList = {caldera::ImageDescription{}, caldera::ImageDescription{},
                                         caldera::ImageDescription{}, caldera::ImageDescription{}};

    cd::ImageTrackingTable const trackingTable = {{caldera::ImageID{1, 0}, cd::ImageTrackedInfo{}},
                                                  {caldera::ImageID{1, 1}, cd::ImageTrackedInfo{}},
                                                  {caldera::ImageID{1, 2}, cd::ImageTrackedInfo{}},
                                                  {caldera::ImageID{1, 3}, cd::ImageTrackedInfo{}}};

    failAtCreationIndex = 2;

    auto const resources = cd::create_images(dummyDevice, dummyFamilies, descriptionList, trackingTable);

    EXPECT_FALSE(resources.has_value());
    EXPECT_EQ(resources.error(), vk::Result::eErrorOutOfDeviceMemory);
    EXPECT_EQ(createdImageCount, 2);
    EXPECT_EQ(destroyedImageCount, 2);
}

TEST_F(GraphAllocatorTest, BufferLifecycle) {
    std::vector const descriptionList = {caldera::BufferDescription{}, caldera::BufferDescription{},
                                         caldera::BufferDescription{}, caldera::BufferDescription{}};

    cd::BufferTrackingTable const trackingTable = {{caldera::BufferID{1, 0}, cd::BufferTrackedInfo{}},
                                                   {caldera::BufferID{1, 1}, cd::BufferTrackedInfo{}},
                                                   {caldera::BufferID{1, 2}, cd::BufferTrackedInfo{}},
                                                   {caldera::BufferID{1, 3}, cd::BufferTrackedInfo{}}};

    auto const resources = cd::create_buffers(dummyDevice, dummyFamilies, descriptionList, trackingTable);
    EXPECT_TRUE(resources.has_value());
    EXPECT_TRUE(resources->size() == descriptionList.size());

    ASSERT_TRUE(createdBufferCount != 0);
    EXPECT_TRUE(resources->size() == createdBufferCount);

    cd::destroy_buffers(dummyDevice, *resources);
    EXPECT_TRUE(destroyedBufferCount == createdBufferCount);
}

TEST_F(GraphAllocatorTest, BufferCreationFailureRollback) {
    std::vector const descriptionList = {caldera::BufferDescription{}, caldera::BufferDescription{},
                                         caldera::BufferDescription{}, caldera::BufferDescription{}};

    cd::BufferTrackingTable const trackingTable = {{caldera::BufferID{1, 0}, cd::BufferTrackedInfo{}},
                                                   {caldera::BufferID{1, 1}, cd::BufferTrackedInfo{}},
                                                   {caldera::BufferID{1, 2}, cd::BufferTrackedInfo{}},
                                                   {caldera::BufferID{1, 3}, cd::BufferTrackedInfo{}}};

    failAtCreationIndex = 1;

    auto const resources = cd::create_buffers(dummyDevice, dummyFamilies, descriptionList, trackingTable);

    EXPECT_FALSE(resources.has_value());
    EXPECT_EQ(resources.error(), vk::Result::eErrorOutOfDeviceMemory);
    EXPECT_EQ(createdBufferCount, 1);
    EXPECT_EQ(destroyedBufferCount, 1);
}