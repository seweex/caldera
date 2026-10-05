#pragma once

#include "fwd.h"

namespace caldera
{
    class GraphCompiler
    {
    public:
        void compile(GraphIR const& ir, RenderGraph& graph);
    };
}
