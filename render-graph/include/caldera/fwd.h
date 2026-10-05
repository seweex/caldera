#pragma once

#include <cstdint>

namespace caldera
{
    class RenderGraph;

    enum class FamilyType : uint16_t;
    struct FamilyNumbers;

    enum class ImageUsage;
    enum class BufferUsage;

    struct ImageState;
    struct BufferState;

    struct ImageDescription;
    struct BufferDescription;

    struct ImageTag;
    struct BufferTag;

    template <class TagTy>
    struct BasicResourceID;

    template <class TagTy>
    struct BasicResourceVersion;

    using ImageID = BasicResourceID<ImageTag>;
    using BufferID = BasicResourceID<BufferTag>;

    using ImageVersion = BasicResourceVersion<ImageTag>;
    using BufferVersion = BasicResourceVersion<BufferTag>;

    class ResourceMap;
    class ResourceStateManager;

    struct ImageBarrier;
    struct BufferBarrier;

    class GraphCompiler;
    class GraphAllocator;
    class GraphExecutor;

    class GraphIR;

    class CommandBufferDistributor;
    class SemaphoreMap;

    class GraphDeclarator;
    class PassDeclarator;
}