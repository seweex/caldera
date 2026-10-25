#pragma once

#include <caldera/fwd.hpp>

namespace caldera::detail
{
    class GraphCompiler
    {
    public:
        [[nodiscard]] CompiledGraph compile(
            FamilyNumbers const& families, GraphIR ir);
    };
}
