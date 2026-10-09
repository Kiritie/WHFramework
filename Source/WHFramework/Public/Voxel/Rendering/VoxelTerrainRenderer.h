#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Rendering/VoxelFineHandoff.h"
#include "Voxel/Rendering/VoxelTerrainCoarseMesh.h"
#include "Voxel/Task/VoxelTerrainResources.h"
#include "Voxel/Task/VoxelTerrainWorkSelection.h"

class UWorld;
class UVoxelModule;
class FVoxelTaskScheduler;
class FVoxelTerrainRenderChunk;
struct FVoxelTerrainRendererState;
struct FVoxelFineBoundarySnapshot;
struct FVoxelFineShardGeometry;

struct WHFRAMEWORK_API FVoxelTerrainAtomDrawRange
{
	TSharedPtr<const FVoxelTerrainRenderChunk, ESPMode::ThreadSafe> Chunk;
	int32 FirstTriangle = 0;
	int32 TriangleCount = 0;
};

struct WHFRAMEWORK_API FVoxelTerrainCoarseDrawPiece
{
	TSharedPtr<const FVoxelTerrainCoarsePatchPiece, ESPMode::ThreadSafe> Piece;
	TSharedPtr<const FVoxelTerrainRenderChunk, ESPMode::ThreadSafe> Chunk;
	TSharedPtr<const FVoxelTerrainCoarseDrawPiece, ESPMode::ThreadSafe> Next;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
};

struct WHFRAMEWORK_API FVoxelTerrainCoarseDrawNode
{
	TSharedPtr<const FVoxelTerrainCoarsePatchNode, ESPMode::ThreadSafe> Patch;
	TSharedPtr<const FVoxelTerrainCoarseDrawNode, ESPMode::ThreadSafe> Children[2];
	TSharedPtr<const FVoxelTerrainCoarseDrawPiece, ESPMode::ThreadSafe> FirstPiece;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
};

// 原 Coarse 正文持有退休尝试；仅 GT 修改，不参与几何身份或 RT 绘制。
struct WHFRAMEWORK_API FVoxelTerrainCoarseRetirementAttempt
{
	FVoxelGenerationConsumerKey Consumer;
	FGuid Transaction;
	EVoxelTerrainResourceAdmission Admission = EVoxelTerrainResourceAdmission::None;
	FVoxelTerrainResourceLimits Limits;
	uint64 CapacityVersion = 0;
	uint64 FrontendVersion = 0;
	double FirstQueuedSeconds = -1.0;
};

// 粗层按实际空间 Key 保持一份原始网格；覆盖 Atom 不分配独立粗层几何。
struct WHFRAMEWORK_API FVoxelTerrainRenderCoarse final : FVoxelTaskCustomPayload
{
	FGuid SourceId;
	bool bBoundaryOnly = false;
	TSharedPtr<const FVoxelTerrainCoarseMesh, ESPMode::ThreadSafe> Geometry;
	FVoxelFineOwnerEvidence Evidence;
	TArray<TSharedPtr<const FVoxelTerrainRenderChunk, ESPMode::ThreadSafe>> SourceChunks;
	TArray<TSharedPtr<const FVoxelTerrainCoarseDrawNode, ESPMode::ThreadSafe>> DrawRoots;
	TSharedPtr<const FVoxelTerrainCoarsePatch, ESPMode::ThreadSafe> Patch;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
	FBox OwnershipBounds = FBox(ForceInit);
	mutable TAtomic<bool> bResidentResourcesReady{false};
	mutable FVoxelTerrainCoarseRetirementAttempt RetirementAttempt;
	bool IsResourceReady() const;
	virtual uint64 GetAllocatedBytes() const override;
};

struct WHFRAMEWORK_API FVoxelTerrainCoarseMutation
{
	TSharedPtr<const FVoxelTerrainRenderCoarse, ESPMode::ThreadSafe> Item;
	bool bRemove = false;
};

// 仅来自真实 RT 回执的只读覆盖根；跨线程读者保持完整几何租约。
class WHFRAMEWORK_API IVoxelTerrainPresentedSnapshot
{
public:
	virtual ~IVoxelTerrainPresentedSnapshot() = default;
	virtual uint64 GetWorldEpoch() const = 0;
	virtual uint64 GetSerial() const = 0;
	virtual int32 GetFineAtomCount() const = 0;
	virtual int32 GetOwnerAtomCount(EVoxelFineOwnerKind InKind) const = 0;
	virtual int32 GetCoarseOwnerCount(EVoxelFineOwnerKind InKind) const = 0;
	virtual const FVoxelFineOwnerEvidence* Find(const FIntVector& InSection) const = 0;
};

// 一个 Atom 的绘制引用；几何和边界的实体租约可以由多个 Atom 共享。
struct WHFRAMEWORK_API FVoxelTerrainRenderAtom final : FVoxelTaskCustomPayload
{
	FIntVector Section = FIntVector::ZeroValue;
	FVoxelFineOwnerEvidence Evidence;
	TArray<TSharedPtr<const FVoxelTerrainRenderChunk, ESPMode::ThreadSafe>> Chunks;
	TArray<FVoxelTerrainAtomDrawRange> DrawRanges;
	TSharedPtr<const FVoxelFineShardGeometry, ESPMode::ThreadSafe> FineGeometry;
	bool bUseDrawRanges = false;
	TArray<TSharedPtr<const FVoxelFineBoundarySnapshot, ESPMode::ThreadSafe>> Boundaries;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
	mutable TAtomic<bool> bResidentResourcesReady{false};
	bool HasMatchingFineInputs() const;
	bool IsResourceReady() const;
	virtual uint64 GetAllocatedBytes() const override;
};

// GT 发布事务的唯一入口；Actual Presented 由 Handoff 的 RT 回执镜像持有。
class WHFRAMEWORK_API FVoxelTerrainRenderer
{
public:
	FVoxelTerrainRenderer(UVoxelModule& InModule, FVoxelTaskScheduler& InScheduler, uint64 InWorldEpoch);
	~FVoxelTerrainRenderer();
	FVoxelTerrainRenderer(const FVoxelTerrainRenderer&) = delete;
	FVoxelTerrainRenderer& operator=(const FVoxelTerrainRenderer&) = delete;
	EVoxelTerrainResourceAdmission GetNativeStateAdmission() const;
	uint64 GetCoarseAdmissionVersion() const;
	void VisitNativeControlSources(FVoxelGenerationRequiredSourceScope::FVisit InVisit) const;
	static void VisitCoarseSources(const FVoxelTerrainRenderCoarse& InCoarse, const FVoxelTerrainResources& InResources,
		FVoxelGenerationRequiredSourceScope::FVisit InVisit);
	static void VisitMeshSources(const FVoxelSectionMeshResult& InMesh, const FVoxelTerrainResources& InResources,
		FVoxelGenerationRequiredSourceScope::FVisit InVisit);
	static void VisitFineSources(const FVoxelFineShardGeometry& InGeometry, const FVoxelTerrainResources& InResources,
		FVoxelGenerationRequiredSourceScope::FVisit InVisit);
	TSharedPtr<const FVoxelTerrainRenderChunk, ESPMode::ThreadSafe> PrepareChunk(
		const FVoxelGenerationConsumerKey& InConsumer,
		TSharedPtr<const FVoxelSectionMeshResult, ESPMode::ThreadSafe> InMesh,
		int32 InBatch, const FVector& InWorldOrigin, double InWorldScale,
		int32 InFirstTriangle = 0, int32 InTriangleCount = INDEX_NONE,
		EVoxelTerrainResourceAdmission* OutAdmission = nullptr);
	void AppendSourceChunk(FVoxelTerrainRenderCoarse& InItem, int32 InBatch,
		TSharedPtr<const FVoxelTerrainRenderChunk, ESPMode::ThreadSafe>& InOutLast,
		TSharedPtr<const FVoxelTerrainRenderChunk, ESPMode::ThreadSafe> InChunk);
	bool PrepareAtomTransaction(
		TConstArrayView<TSharedPtr<const FVoxelTerrainRenderAtom, ESPMode::ThreadSafe>> InAtoms,
		uint64 InDemandRevision, bool bInRequiresCollision,
		TFunction<bool(const FIntVector&, const FVoxelFineOwnerEvidence&)> InIsCurrent,
		TFunction<bool(const FIntVector&, const FVoxelFineOwnerEvidence&)> InIsCollisionCurrent,
		TFunction<void()> InOnSubmitted, FGuid& OutTransaction,
		const FVoxelTerrainServicePriority& InPriority = FVoxelTerrainServicePriority(),
		double InFirstQueuedSeconds = -1.0,
		EVoxelTerrainResourceAdmission* OutAdmission = nullptr,
		const FVoxelGenerationConsumerKey& InConsumer = {});
	bool PrepareCoarseTransaction(
		TSharedPtr<const FVoxelTerrainRenderCoarse, ESPMode::ThreadSafe> InCoarse,
		bool bInRemove,
		TFunction<bool()> InIsCurrent,
		TFunction<void()> InOnSubmitted,
		FGuid& OutTransaction,
		const FVoxelTerrainServicePriority& InPriority,
		double InFirstQueuedSeconds,
		EVoxelTerrainResourceAdmission* OutAdmission = nullptr,
		const FVoxelGenerationConsumerKey& InConsumer = {},
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InCallbackLease = nullptr);
	bool PrepareCoarseTransaction(
		TConstArrayView<FVoxelTerrainCoarseMutation> InChanges,
		TFunction<bool()> InIsCurrent,
		TFunction<void()> InOnSubmitted,
		FGuid& OutTransaction,
		const FVoxelTerrainServicePriority& InPriority,
		double InFirstQueuedSeconds,
		EVoxelTerrainResourceAdmission* OutAdmission = nullptr,
		const FVoxelGenerationConsumerKey& InConsumer = {},
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InCallbackLease = nullptr);
	bool IsTransactionPending(const FGuid& InTransaction) const;
	// 消费原事务的永久失败；调用方保留旧表示且不为同一请求自动构造新事务。
	bool ConsumeTransactionFailure(const FGuid& InTransaction);
	void PollRenderReceipts();
	void AdvanceFramePublication();
	void AdvanceRetirement(TFunction<void(const FIntVector&)> InOnRetired);
	const FVoxelFinePresentedAtom* FindPresentedAtom(const FIntVector& InSection) const;
	TSharedPtr<const IVoxelTerrainPresentedSnapshot, ESPMode::ThreadSafe> CapturePresentedSnapshot() const;
	bool HasPresentedCoverage(const FBox& InCellBounds) const;
	bool RequiresF0RuntimeData(const FIntVector& InSection,
		TFunctionRef<bool(const FIntVector&, const FVoxelFineOwnerEvidence&)> InIsReplacementCurrent) const;
	bool IsLocked(const FIntVector& InSection) const;
	bool IsBusy() const;
	void Reset();

private:
	bool EnsureState();

	TWeakObjectPtr<UVoxelModule> BoundModule;
	TWeakObjectPtr<UWorld> BoundWorld;
	FVoxelTaskScheduler* BoundScheduler = nullptr;
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> BoundResources;
	FVoxelGenerationConsumerKey BootstrapConsumer;
	EVoxelTerrainResourceAdmission BootstrapAdmission = EVoxelTerrainResourceAdmission::None;
	uint64 BootstrapCapacityVersion = 0;
	bool bStopped = false;
	TSharedPtr<FVoxelTerrainRendererState, ESPMode::ThreadSafe> State;
};
