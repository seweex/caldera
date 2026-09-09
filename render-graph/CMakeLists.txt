
set(CALDERA_RENDER_GRAPH_CPP_FILES
    )

add_library(caldera-render-graph STATIC ${CALDERA_RENDER_GRAPH_CPP_FILES})

set_target_properties(caldera-render-graph PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)