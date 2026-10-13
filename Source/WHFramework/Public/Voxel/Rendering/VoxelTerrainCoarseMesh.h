#pragma once

#include "CoreMinimal.h"
#include "Voxel/Geometry/VoxelSectionMesher.h"
#include "Voxel/Task/VoxelTerrainResources.h"

struct WHFRAMEWORK_API FVoxelTerrainCoarseRange
{
	int32 Batch = INDEX_NONE;
	int32 FirstTriangle = 0;
	int32 TriangleCount = 0;
	int32 Children[2] = {INDEX_NONE, INDEX_NONE};
	FBox WorldCellBounds = FBox(ForceInit);
	bool IsLeaf() const { return Children[0] == INDEX_NONE; }
};

// 粗层保留一份原始几何及索引范围。只有与 Fine 交界相交的范围才参与裁切。
struct WHFRAMEWORK_API FVoxelTerrainCoarseMesh final : FVoxelTaskCustomPayload
{
	FVoxelTaskStamp Stamp;
	FGuid GeometryIdentity = FGuid::NewGuid();
	TSharedPtr<const FVoxelSectionMeshResult, ESPMode::ThreadSafe> Source;
	FVector WorldCellOrigin = FVector::ZeroVector;
	double CellScale = 1.0;
	TArray<FVoxelTerrainCoarseRange> Ranges;
	TArray<int32> Roots;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
	void VisitIntersecting(const FBox& InWorldCellBounds, TFunctionRef<void(int32)> InVisit) const;
	virtual uint64 GetAllocatedBytes() const override;
	virtual uint64 GetResourceAccountedBytes() const override;
};

struct WHFRAMEWORK_API FVoxelTerrainCoarsePatchPiece
{
	TSharedPtr<const FVoxelSectionMeshResult, ESPMode::ThreadSafe> Mesh;
	TSharedPtr<FVoxelTerrainCoarsePatchPiece, ESPMode::ThreadSafe> Next;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
};

// 空指针表示直接复用原始索引范围；存在且没有 Piece 的叶子表示已被覆盖。
struct WHFRAMEWORK_API FVoxelTerrainCoarsePatchNode
{
	int32 Range = INDEX_NONE;
	TSharedPtr<const FVoxelTerrainCoarsePatchNode, ESPMode::ThreadSafe> Children[2];
	TSharedPtr<const FVoxelTerrainCoarsePatchPiece, ESPMode::ThreadSafe> FirstPiece;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
};

struct WHFRAMEWORK_API FVoxelTerrainCoarsePatch final : FVoxelTaskCustomPayload
{
	TSharedPtr<const FVoxelTerrainCoarseMesh, ESPMode::ThreadSafe> Geometry;
	TArray<TSharedPtr<const FVoxelTerrainCoarsePatchNode, ESPMode::ThreadSafe>> Roots;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
	uint64 NewBytes = 0;
	virtual uint64 GetAllocatedBytes() const override;
	virtual uint64 GetResourceAccountedBytes() const override;
};

// 覆盖来自同一冻结呈现根。包含容量租约，不能借用 Desired 的临时数组。
struct WHFRAMEWORK_API FVoxelTerrainCoarseExclusions
{
	TArray<FBox> Boxes;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
};

enum class EVoxelTerrainCoarseMeshProgress : uint8
{
	Working,
	Complete,
	CapacityBlocked,
	Canceled,
	Failed
};

// 每个叶子最多 32 个原始三角形；空间跨度不会扩大初次构建的顶点或 Atom 数量。
class WHFRAMEWORK_API FVoxelTerrainCoarseMeshBuilder
{
public:
	FVoxelTerrainCoarseMeshBuilder(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer, bool bInPublicationOutput = false, bool bInAllowResidentOverlap = false);
	bool Initialize(TSharedPtr<const FVoxelSectionMeshResult, ESPMode::ThreadSafe> InMesh,
		const FVector& InWorldCellOrigin, double InCellScale, const FVoxelTaskStamp& InStamp, FString& OutError);
	EVoxelTerrainCoarseMeshProgress Advance(int32 InMaximumSteps, const TAtomic<bool>& InCancel, FString& OutError);
	TSharedPtr<const FVoxelTerrainCoarseMesh, ESPMode::ThreadSafe> TakeResult();

private:
	struct FWalk
	{
		int32 Node = INDEX_NONE;
		int32 Triangle = 0;
		bool bExpanded = false;
	};
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	TSharedPtr<const FVoxelSectionMeshResult, ESPMode::ThreadSafe> Input;
	TSharedPtr<FVoxelTerrainCoarseMesh, ESPMode::ThreadSafe> Result;
	TArray<FWalk, TInlineAllocator<64>> Walk;
	FVoxelTaskStamp Stamp;
	FVector Origin = FVector::ZeroVector;
	double Scale = 1.0;
	int32 MaximumNodes = 0;
	int32 MaximumRoots = 0;
	int32 BatchCursor = 0;
	bool bComplete = false;
	bool bPublicationOutput = false;
	bool bAllowResidentOverlap = false;
};

// 只重建变化区相交叶子。每次 continuation 推进一个节点、三角形或裁切片段。
class WHFRAMEWORK_API FVoxelTerrainCoarsePatchBuilder
{
public:
	FVoxelTerrainCoarsePatchBuilder(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer, bool bInPublicationOutput = false, bool bInAllowResidentOverlap = false);
	~FVoxelTerrainCoarsePatchBuilder();
	bool Initialize(TSharedPtr<const FVoxelTerrainCoarseMesh, ESPMode::ThreadSafe> InGeometry,
		TSharedPtr<const FVoxelTerrainCoarsePatch, ESPMode::ThreadSafe> InBase,
		TSharedPtr<const FVoxelTerrainCoarseExclusions, ESPMode::ThreadSafe> InExclusions,
		TConstArrayView<FBox> InChangedBounds, FString& OutError);
	EVoxelTerrainCoarseMeshProgress Advance(int32 InMaximumSteps, const TAtomic<bool>& InCancel, FString& OutError);
	TSharedPtr<const FVoxelTerrainCoarsePatch, ESPMode::ThreadSafe> TakeResult();

private:
	struct FState;
	TSharedPtr<FState, ESPMode::ThreadSafe> State;
};
