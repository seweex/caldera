#pragma once

#include <cstdint>

namespace caldera
{
    class RenderGraph;

    enum class FamilyType;
    struct FamilyNumbers;

    enum class ImageUsage;
    enum class BufferUsage;

    struct ImageState;
    struct BufferState;

    struct ImageDescription;
    struct BufferDescription;
    struct ImageViewDescription;

    struct ImageTag;
    struct BufferTag;

    template <class TagTy>
    struct BasicResourceID;

    using ImageID = BasicResourceID<ImageTag>;
    using BufferID = BasicResourceID<BufferTag>;

    class ResourceMap;
    class ResourceStateManager;

    struct ImageBarrier;
    struct BufferBarrier;

    class GraphCompiler;
    class GraphAllocator;
    class GraphExecutor;

    struct GraphIR;

    class CommandBufferDistributor;
    class SemaphoreMap;

    class GraphDeclarator;
    class PassDeclarator;
}