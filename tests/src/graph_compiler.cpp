#include <gtest/gtest.h>
#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <span>
#include <vector>

#include <caldera/structs.hpp>
#include <detail/graph_compiler.hpp>

namespace cd = caldera::detail;

class GraphCompilerTest : public ::testing::Test {
  protected:
    static constexpr caldera::FamilyNumbers dummyFamilies = {.graphics = 0, .transfer = 1, .compute = 2};

    struct BufferVersionEntry {
        cd::BufferTouch writing;
        std::vector<cd::BufferTouch> readingList;
    };

    struct ImageVersionEntry {
        cd::ImageTouch writing;
        std::vector<cd::ImageTouch> readingList;
    };
};

TEST_F(GraphCompilerTest, SortReadingsPicksSameFamilyAndLayoutFirst) {
    cd::ImageTouch const writing{.stages = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                 .access = vk::AccessFlagBits2::eColorAttachmentWrite,
                                 .layout = vk::ImageLayout::eShaderReadOnlyOptimal,
                                 .family = caldera::FamilyType::graphics,
                                 .passIdx = 0};

    std::vector readings = {cd::ImageTouch{.stages = vk::PipelineStageFlagBits2::eComputeShader,
                                           .access = vk::AccessFlagBits2::eShaderRead,
                                           .layout = vk::ImageLayout::eGeneral,
                                           .family = caldera::FamilyType::compute,
                                           .passIdx = 1},
                            cd::ImageTouch{.stages = vk::PipelineStageFlagBits2::eFragmentShader,
                                           .access = vk::AccessFlagBits2::eShaderRead,
                                           .layout = vk::ImageLayout::eShaderReadOnlyOptimal,
                                           .family = caldera::FamilyType::graphics,
                                           .passIdx = 2}};

    cd::sort_readings(writing, readings, dummyFamilies);

    EXPECT_EQ(readings.front().passIdx, 2);
    EXPECT_EQ(readings.back().passIdx, 1);
}

TEST_F(GraphCompilerTest, DeduplicateAdjacencyListRemovesDuplicateChildrenAndMergesStages) {
    cd::GraphAdjacencyList adjacency;
    adjacency.passChildren = {{1, 2, 1}, {}, {}};

    adjacency.passParents = {{},
                             {caldera::PassDependency{.otherPass = 0,
                                                      .srcStages = vk::PipelineStageFlagBits2::eVertexShader,
                                                      .dstStages = vk::PipelineStageFlagBits2::eFragmentShader},
                              caldera::PassDependency{.otherPass = 0,
                                                      .srcStages = vk::PipelineStageFlagBits2::eTransfer,
                                                      .dstStages = vk::PipelineStageFlagBits2::eComputeShader}},
                             {caldera::PassDependency{.otherPass = 0,
                                                      .srcStages = vk::PipelineStageFlagBits2::eAllCommands,
                                                      .dstStages = vk::PipelineStageFlagBits2::eAllCommands}}};

    cd::deduplicate_adjacency_list(adjacency);

    ASSERT_EQ(adjacency.passChildren[0].size(), 2);
    EXPECT_EQ(adjacency.passChildren[0][0], 1);
    EXPECT_EQ(adjacency.passChildren[0][1], 2);

    ASSERT_EQ(adjacency.passParents[1].size(), 1);
    EXPECT_EQ(adjacency.passParents[1][0].otherPass, 0);
    EXPECT_EQ(adjacency.passParents[1][0].srcStages,
              vk::PipelineStageFlagBits2::eVertexShader | vk::PipelineStageFlagBits2::eTransfer);
    EXPECT_EQ(adjacency.passParents[1][0].dstStages,
              vk::PipelineStageFlagBits2::eFragmentShader | vk::PipelineStageFlagBits2::eComputeShader);
}

TEST_F(GraphCompilerTest, TopologicalSortResolvesSimpleChain) {
    cd::GraphAdjacencyList const adjacency{
        .passChildren = {{1}, {2}, {}},
        .passParents = {{}, {caldera::PassDependency{.otherPass = 0}}, {caldera::PassDependency{.otherPass = 1}}}};

    std::vector<cd::PassDeclaration> const passes = {{.family = caldera::FamilyType::graphics},
                                                     {.family = caldera::FamilyType::graphics},
                                                     {.family = caldera::FamilyType::graphics}};

    auto const order = cd::topological_sort(adjacency, dummyFamilies, passes);

    ASSERT_EQ(order.size(), 3);
    EXPECT_EQ(order[0], 0);
    EXPECT_EQ(order[1], 1);
    EXPECT_EQ(order[2], 2);
}

TEST_F(GraphCompilerTest, TopologicalSortRespectsSlotAffinityOnBranching) {
    cd::GraphAdjacencyList const adjacency{
        .passChildren = {{1, 2}, {}, {}},
        .passParents = {{}, {caldera::PassDependency{.otherPass = 0}}, {caldera::PassDependency{.otherPass = 0}}}};

    std::vector<cd::PassDeclaration> const passes = {{.family = caldera::FamilyType::graphics},
                                                     {.family = caldera::FamilyType::graphics},
                                                     {.family = caldera::FamilyType::compute}};

    auto const order = cd::topological_sort(adjacency, dummyFamilies, passes);

    ASSERT_EQ(order.size(), 3);
    EXPECT_EQ(order[0], 0);
    EXPECT_EQ(order[1], 1);
    EXPECT_EQ(order[2], 2);
}

TEST_F(GraphCompilerTest, AppendAllowedResourcesCalculatesOffsetsAndSizes) {
    cd::CompiledGraph graph;
    graph.compiledPasses.resize(2);

    caldera::BufferID constexpr bufferA{.isTransient = 1, .index = 0};
    caldera::BufferID constexpr bufferB{.isTransient = 1, .index = 1};

    std::vector<std::pair<caldera::BufferID, std::vector<BufferVersionEntry>>> const versionTable = {
        {bufferA, {BufferVersionEntry{.writing = {.passIdx = 0}, .readingList = {{.passIdx = 1}}}}},
        {bufferB, {BufferVersionEntry{.writing = {.passIdx = 0}, .readingList = {}}}}};

    std::vector<uint32_t> const passOrder = {0, 1};

    cd::append_with_allowed_resources<caldera::BufferTag>(
        graph, &cd::CompiledGraph::allowedBuffers, &cd::CompiledPass::allowedBufferOffset,
        &cd::CompiledPass::allowedBufferSize, versionTable, passOrder);

    ASSERT_EQ(graph.allowedBuffers.size(), 3);

    EXPECT_EQ(graph.compiledPasses[0].allowedBufferOffset, 0);
    EXPECT_EQ(graph.compiledPasses[0].allowedBufferSize, 2);

    EXPECT_EQ(graph.compiledPasses[1].allowedBufferOffset, 2);
    EXPECT_EQ(graph.compiledPasses[1].allowedBufferSize, 1);

    EXPECT_EQ(graph.allowedBuffers[0], bufferA);
    EXPECT_EQ(graph.allowedBuffers[1], bufferB);
    EXPECT_EQ(graph.allowedBuffers[2], bufferA);
}

TEST_F(GraphCompilerTest, AppendPassDependenciesRemapsToExecutionIndices) {
    cd::CompiledGraph graph;
    graph.compiledPasses.resize(3);

    cd::GraphAdjacencyList const adjacency{
        .passChildren = {{2}, {}, {1}},
        .passParents = {{},
                        {caldera::PassDependency{.otherPass = 2,
                                                 .srcStages = vk::PipelineStageFlagBits2::eTransfer,
                                                 .dstStages = vk::PipelineStageFlagBits2::eComputeShader}},
                        {caldera::PassDependency{.otherPass = 0,
                                                 .srcStages = vk::PipelineStageFlagBits2::eVertexShader,
                                                 .dstStages = vk::PipelineStageFlagBits2::eFragmentShader}}}};

    std::vector<uint32_t> const passOrder = {0, 2, 1};

    cd::append_with_pass_dependencies(graph, adjacency, passOrder);

    ASSERT_EQ(graph.passDependencies.size(), 2);

    EXPECT_EQ(graph.compiledPasses[0].passDependencyOffset, 0);
    EXPECT_EQ(graph.compiledPasses[0].passDependencySize, 0);

    EXPECT_EQ(graph.compiledPasses[2].passDependencyOffset, 0);
    EXPECT_EQ(graph.compiledPasses[2].passDependencySize, 1);
    EXPECT_EQ(graph.passDependencies[0].srcStages, vk::PipelineStageFlagBits2::eVertexShader);

    EXPECT_EQ(graph.compiledPasses[1].passDependencyOffset, 1);
    EXPECT_EQ(graph.compiledPasses[1].passDependencySize, 1);
    EXPECT_EQ(graph.passDependencies[1].srcStages, vk::PipelineStageFlagBits2::eTransfer);
}