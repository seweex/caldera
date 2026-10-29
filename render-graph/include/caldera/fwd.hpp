#pragma once

#include <unordered_map>
#include <vector>

namespace caldera {
struct ImageTag;
struct BufferTag;

template <class TagTy> struct BasicResourceID;

template <class TagTy> struct BasicResourceVersion;

using ImageID = BasicResourceID<ImageTag>;
using BufferID = BasicResourceID<BufferTag>;

using ImageVersion = BasicResourceVersion<ImageTag>;
using BufferVersion = BasicResourceVersion<BufferTag>;

using ImageResourceVersion = BasicResourceVersion<ImageTag>;
using BufferResourceVersion = BasicResourceVersion<BufferTag>;

enum class MappingPolicy;
enum class FamilyType;
enum class ImageUsage;
enum class BufferUsage;

struct FamilyNumbers;
struct FamilyPassCount;
struct ImageDescription;
struct BufferDescription;
struct PassDependency;
struct ImageState;
struct BufferState;
struct GraphIR;
struct SubmittableGraph;
struct ExecutionContext;
struct ExecutionMemory;
struct SemaphoreSyncPoint;

class CommandBufferDistributor;
class GraphDeclarator;
class GraphExecutor;
class PassDeclarator;
class RenderGraph;
class ResourceMap;
class ResourceStateManager;
class GraphProperties;
} // namespace caldera

namespace caldera::detail {
enum class HazardType;
enum class AccessKind;

struct ImageTouch;
struct BufferTouch;
struct ImageVersion;
struct BufferVersion;
struct ImageBarrier;
struct BufferBarrier;
struct ImageTrackedInfo;
struct BufferTrackedInfo;
struct CommandFamily;
struct PassDeclaration;
struct GraphAdjacencyList;
struct GraphResources;
struct CompiledPass;
struct CompiledGraph;

class GraphAllocator;

using ImageVersionTable = std::unordered_map<ImageID, std::vector<ImageVersion>>;
using BufferVersionTable = std::unordered_map<BufferID, std::vector<BufferVersion>>;

using ImageTrackingTable = std::unordered_map<ImageID, ImageTrackedInfo>;
using BufferTrackingTable = std::unordered_map<BufferID, BufferTrackedInfo>;
} // namespace caldera::detail