#pragma once

#include <caldera/fwd.h>
#include <caldera/structs.hpp>

namespace caldera::detail
{
    class GraphCompiler
    {
    public:
        [[nodiscard]] CompiledGraph compile(
            FamilyNumbers const& families, GraphIR ir);
    };
}
