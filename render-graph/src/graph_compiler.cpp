#include "detail/graph_compiler.hpp"

namespace caldera::detail {
    CompiledGraph GraphCompiler::compile(
        FamilyNumbers const &families,
        GraphIR ir)
    {
        sort_versions(ir.imageVersions, families);
        sort_versions(ir.bufferVersions, families);

        GraphAdjacencyList adjacency;
        adjacency.passParents.resize(ir.passes.size());
        adjacency.passChildren.resize(ir.passes.size());

        append_adjacency_list(adjacency, families, ir.imageVersions);
        append_adjacency_list(adjacency, families, ir.bufferVersions);
        deduplicate_adjacency_list(adjacency);

        auto const order = topological_sort(adjacency, families, ir.passes);
        return generate_compiled_graph(ir, families, adjacency, order);
    }
}