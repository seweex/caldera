#pragma once

#include <vector>
#include "fwd.h"
#include "structs.h"

namespace caldera
{
    struct SubmittablePass
    {
        FamilyType family;
        vk::CommandBuffer cmd;

        std::vector<vk::SemaphoreSubmitInfo> signalPasses;
        std::vector<vk::SemaphoreSubmitInfo> waitPasses;
    };

    struct SubmittableGraph {
        std::vector<SubmittablePass> passes;
    };

    class GraphExecutor
    {
    public:
        [[nodiscard]] SubmittableGraph execute(
            RenderGraph const& graph,
            ExecutionContext const& context);
    };
}
