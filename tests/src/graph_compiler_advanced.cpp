#include <gtest/gtest.h>
#include <vulkan/vulkan.hpp>

#include <vector>
#include <cstdint>

#include <caldera/structs.hpp>
#include <detail/graph_compiler.hpp>

namespace cd = caldera::detail;

class GraphCompilerAdvancedTest : public ::testing::Test
{
protected:
    static constexpr caldera::FamilyNumbers dummyFamilies = {
        .graphics = 0,
        .transfer = 1,
        .compute = 2
    };
};

TEST_F(GraphCompilerAdvancedTest, MakeInitialImageBarrierSetsUndefinedLayoutAndFullRange)
{
    caldera::ImageID constexpr imageId { .isTransient = 0, .index = 7 };

    cd::ImageTrackedInfo const trackedInfo {
        .firstTouch = {
            .stages = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .access = vk::AccessFlagBits2::eColorAttachmentWrite,
            .layout = vk::ImageLayout::eColorAttachmentOptimal,
            .family = caldera::FamilyType::graphics,
            .passIdx = 0
        },
        .lastTouch = {},
        .usages = vk::ImageUsageFlagBits::eColorAttachment,
        .aspect = vk::ImageAspectFlagBits::eColor
    };

    auto const barrier = cd::make_initial_barrier(imageId, dummyFamilies, trackedInfo);

    EXPECT_EQ(barrier.id, imageId);
    EXPECT_EQ(barrier.oldLayout, vk::ImageLayout::eUndefined);
    EXPECT_EQ(barrier.newLayout, vk::ImageLayout::eColorAttachmentOptimal);
    EXPECT_EQ(barrier.srcFamilyIdx, vk::QueueFamilyIgnored);
    EXPECT_EQ(barrier.dstFamilyIdx, dummyFamilies.get(caldera::FamilyType::graphics));
    EXPECT_EQ(barrier.dstStages, vk::PipelineStageFlagBits2::eColorAttachmentOutput);
    EXPECT_EQ(barrier.dstAccess, vk::AccessFlagBits2::eColorAttachmentWrite);
    EXPECT_EQ(barrier.subresourceRange.aspectMask, vk::ImageAspectFlagBits::eColor);
    EXPECT_EQ(barrier.subresourceRange.baseMipLevel, 0);
    EXPECT_EQ(barrier.subresourceRange.levelCount, vk::RemainingMipLevels);
    EXPECT_EQ(barrier.subresourceRange.baseArrayLayer, 0);
    EXPECT_EQ(barrier.subresourceRange.layerCount, vk::RemainingArrayLayers);
}

TEST_F(GraphCompilerAdvancedTest, MakeInitialBufferBarrierSetsWholeRangeAndIgnoredSourceFamily)
{
    caldera::BufferID constexpr bufferId { .isTransient = 0, .index = 3 };

    cd::BufferTrackedInfo const trackedInfo {
        .firstTouch = {
            .stages = vk::PipelineStageFlagBits2::eVertexAttributeInput,
            .access = vk::AccessFlagBits2::eVertexAttributeRead,
            .family = caldera::FamilyType::transfer,
            .passIdx = 1
        },
        .lastTouch = {},
        .usages = vk::BufferUsageFlagBits::eVertexBuffer,
        .mappedAccessPercentage = 0,
        .hasRandomMappedAccess = false,
        .hasMappedReading = false
    };

    auto const barrier = cd::make_initial_barrier(bufferId, dummyFamilies, trackedInfo);

    EXPECT_EQ(barrier.id, bufferId);
    EXPECT_EQ(barrier.srcFamilyIdx, vk::QueueFamilyIgnored);
    EXPECT_EQ(barrier.dstFamilyIdx, dummyFamilies.get(caldera::FamilyType::transfer));
    EXPECT_EQ(barrier.dstStages, vk::PipelineStageFlagBits2:: eVertexAttributeInput);
    EXPECT_EQ(barrier.dstAccess, vk::AccessFlagBits2::eVertexAttributeRead);
    EXPECT_EQ(barrier.offset, 0);
    EXPECT_EQ(barrier.size, vk::WholeSize);
}

TEST_F(GraphCompilerAdvancedTest, AppendAdjacencyListConnectsWriterToReadersAndNextWriter)
{
    cd::GraphAdjacencyList adjacency;
    adjacency.passChildren.resize(4);
    adjacency.passParents.resize(4);

    caldera::BufferID constexpr bufferId { .isTransient = 1, .index = 0 };

    cd::BufferVersionTable const versionTable = {
        {
            bufferId,
            {
                cd::BufferVersion {
                    .writing = {
                        .stages = vk::PipelineStageFlagBits2::eTransfer,
                        .access = vk::AccessFlagBits2::eTransferWrite,
                        .family = caldera::FamilyType::transfer,
                        .passIdx = 0
                    },
                    .readingList = {
                        {
                            .stages = vk::PipelineStageFlagBits2::eComputeShader,
                            .access = vk::AccessFlagBits2::eShaderRead,
                            .family = caldera::FamilyType::compute,
                            .passIdx = 1
                        },
                        {
                            .stages = vk::PipelineStageFlagBits2::eComputeShader,
                            .access = vk::AccessFlagBits2::eShaderRead,
                            .family = caldera::FamilyType::compute,
                            .passIdx = 2
                        }
                    }
                },
                cd::BufferVersion {
                    .writing = {
                        .stages = vk::PipelineStageFlagBits2::eComputeShader,
                        .access = vk::AccessFlagBits2::eShaderWrite,
                        .family = caldera::FamilyType::compute,
                        .passIdx = 3
                    },
                    .readingList = {}
                }
            }
        }
    };

    cd::append_adjacency_list(adjacency, dummyFamilies, versionTable);

    ASSERT_EQ(adjacency.passChildren[0].size(), 2);
    EXPECT_EQ(adjacency.passChildren[0][0], 1);
    EXPECT_EQ(adjacency.passChildren[0][1], 2);

    ASSERT_EQ(adjacency.passParents[1].size(), 1);
    EXPECT_EQ(adjacency.passParents[1][0].otherPass, 0);
    EXPECT_EQ(adjacency.passParents[1][0].srcStages, vk::PipelineStageFlagBits2::eTransfer);
    EXPECT_EQ(adjacency.passParents[1][0].dstStages, vk::PipelineStageFlagBits2::eComputeShader);

    ASSERT_EQ(adjacency.passParents[2].size(), 1);
    EXPECT_EQ(adjacency.passParents[2][0].otherPass, 0);

    ASSERT_EQ(adjacency.passChildren[1].size(), 1);
    EXPECT_EQ(adjacency.passChildren[1][0], 3);

    ASSERT_EQ(adjacency.passChildren[2].size(), 1);
    EXPECT_EQ(adjacency.passChildren[2][0], 3);

    ASSERT_EQ(adjacency.passParents[3].size(), 2);
    EXPECT_EQ(adjacency.passParents[3][0].otherPass, 1);
    EXPECT_EQ(adjacency.passParents[3][1].otherPass, 2);
}

TEST_F(GraphCompilerAdvancedTest, AppendAdjacencyListSplitsLayersOnLayoutMismatch)
{
    cd::GraphAdjacencyList adjacency;
    adjacency.passChildren.resize(3);
    adjacency.passParents.resize(3);

    caldera::ImageID constexpr imageId { .isTransient = 1, .index = 0 };

    cd::ImageVersionTable const versionTable = {
        {
            imageId,
            {
                cd::ImageVersion {
                    .writing = {
                        .stages = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                        .access = vk::AccessFlagBits2::eColorAttachmentWrite,
                        .layout = vk::ImageLayout::eColorAttachmentOptimal,
                        .family = caldera::FamilyType::graphics,
                        .passIdx = 0
                    },
                    .readingList = {
                        {
                            .stages = vk::PipelineStageFlagBits2::eFragmentShader,
                            .access = vk::AccessFlagBits2::eShaderRead,
                            .layout = vk::ImageLayout::eShaderReadOnlyOptimal,
                            .family = caldera::FamilyType::graphics,
                            .passIdx = 1
                        },
                        {
                            .stages = vk::PipelineStageFlagBits2::eComputeShader,
                            .access = vk::AccessFlagBits2::eShaderRead,
                            .layout = vk::ImageLayout::eGeneral,
                            .family = caldera::FamilyType::graphics,
                            .passIdx = 2
                        }
                    }
                }
            }
        }
    };

    cd::append_adjacency_list(adjacency, dummyFamilies, versionTable);

    ASSERT_EQ(adjacency.passChildren[0].size(), 1);
    EXPECT_EQ(adjacency.passChildren[0][0], 1);

    ASSERT_EQ(adjacency.passChildren[1].size(), 1);
    EXPECT_EQ(adjacency.passChildren[1][0], 2);

    ASSERT_EQ(adjacency.passParents[2].size(), 1);
    EXPECT_EQ(adjacency.passParents[2][0].otherPass, 1);
}

TEST_F(GraphCompilerAdvancedTest, AppendAdjacencyListSkipsNextWriterPresentInReadingList)
{
    cd::GraphAdjacencyList adjacency;
    adjacency.passChildren.resize(2);
    adjacency.passParents.resize(2);

    caldera::BufferID constexpr bufferId { .isTransient = 1, .index = 0 };

    cd::BufferVersionTable const versionTable = {
        {
            bufferId,
            {
                cd::BufferVersion {
                    .writing = {
                        .stages = vk::PipelineStageFlagBits2::eTransfer,
                        .access = vk::AccessFlagBits2::eTransferWrite,
                        .family = caldera::FamilyType::transfer,
                        .passIdx = 0
                    },
                    .readingList = {
                        {
                            .stages = vk::PipelineStageFlagBits2::eComputeShader,
                            .access = vk::AccessFlagBits2::eShaderRead,
                            .family = caldera::FamilyType::compute,
                            .passIdx = 1
                        }
                    }
                },
                cd::BufferVersion {
                    .writing = {
                        .stages = vk::PipelineStageFlagBits2::eComputeShader,
                        .access = vk::AccessFlagBits2::eShaderWrite,
                        .family = caldera::FamilyType::compute,
                        .passIdx = 1
                    },
                    .readingList = {}
                }
            }
        }
    };

    cd::append_adjacency_list(adjacency, dummyFamilies, versionTable);

    EXPECT_TRUE(adjacency.passChildren[1].empty());
    ASSERT_EQ(adjacency.passChildren[0].size(), 1);
    EXPECT_EQ(adjacency.passChildren[0][0], 1);

    ASSERT_EQ(adjacency.passParents[1].size(), 1);
    EXPECT_EQ(adjacency.passParents[1][0].otherPass, 0);
}

TEST_F(GraphCompilerAdvancedTest, TopologicalSortHandlesDisjointGraphsDeterministically)
{
    cd::GraphAdjacencyList const adjacency {
        .passChildren = { {}, {}, {} },
        .passParents = { {}, {}, {} }
    };

    std::vector<cd::PassDeclaration> const passes = {
        { .name = "Pass0", .family = caldera::FamilyType::graphics },
        { .name = "Pass1", .family = caldera::FamilyType::compute },
        { .name = "Pass2", .family = caldera::FamilyType::transfer }
    };

    auto const order = cd::topological_sort(adjacency, dummyFamilies, passes);

    ASSERT_EQ(order.size(), 3);
    EXPECT_EQ(order[0], 0);
    EXPECT_EQ(order[1], 1);
    EXPECT_EQ(order[2], 2);
}