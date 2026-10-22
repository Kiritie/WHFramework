#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationPipeline.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Runtime/VoxelOverlaySnapshot.h"
#include "Voxel/Runtime/VoxelFineOverlayRead.h"
#include "Voxel/Rendering/VoxelFineBuildState.h"

namespace VoxelFineShardCapacity
{
	EVoxelTerrainResourceAdmission TryReserve(
		const TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe>& InResources,
		const FVoxelGenerationConsumerKey& InConsumer, EVoxelTerrainResourcePool InPool, uint64 InBytes,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& InOutLease, FString& OutError,
		uint64& OutBeforeVersion, uint64 InAdditionalWorkingBytes = 0, uint64 InAdditionalStagedBytes = 0,
		int32 InAdditionalLeaseCount = 0, EVoxelTerrainResourcePool* OutUnachievablePool = nullptr);
}

struct WHFRAMEWORK_API FVoxelFineShardSamples
{
	FVoxelFineBuildAttempt Attempt;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	TSharedPtr<const void, ESPMode::ThreadSafe> RegistrationLease;
	TSharedPtr<const FVoxelFineOverlayVersionProof, ESPMode::ThreadSafe> OverlayVersion;
	FVoxelSectionSnapshot Snapshot;
	uint64 AllocatedBytes() const;
};

enum class EVoxelFineBuildProgress : uint8
{
	None = 0,
	Working,
	Complete,
	DependencyNotReady,
	Canceled,
	CapacityBlocked,
	Failed,
	ResultTaken
};

// GenerateSection 只作为世界事实采样入口；不分配 FVoxelSection，不构建子 Section Mesh。
class WHFRAMEWORK_API FVoxelFineShardSampler
{
public:
	FVoxelFineShardSampler(TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> InGenerator,
		TSharedRef<const FVoxelOverlaySnapshotSet, ESPMode::ThreadSafe> InOverlay,
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer,
		TSharedPtr<const void, ESPMode::ThreadSafe> InRegistrationLease = nullptr,
		TSharedPtr<const FVoxelFineOverlayVersionProof, ESPMode::ThreadSafe> InOverlayVersion = nullptr);
	bool Initialize(const FVoxelFineBuildAttempt& InAttempt, uint64 InRecipeHash, int32 InWorldMinZ, int32 InWorldMaxZ, FString& OutError);
	EVoxelFineBuildProgress Advance(int32 InMaximumSections, const TAtomic<bool>& InCancel, FString& OutError);
	TSharedPtr<const FVoxelFineShardSamples, ESPMode::ThreadSafe> TakeResult();
	uint64 AllocatedBytes() const;
	const TArray<FIntVector>& GetRequiredSections() const;
	static bool GatherRequiredSections(const FVoxelFineShardKey& InKey, TArray<FIntVector>& OutSections, FString& OutError);

private:
	void VisitResourceSources(FVoxelGenerationRequiredSourceScope::FVisit InVisit) const;
	FVoxelSectionWindow RequiredWindow(const FIntVector& InSection) const;
	void CopySection(const FIntVector& InSection, const FVoxelSectionWindow& InWindow, TConstArrayView<FVoxelBlockState> InBlocks);
	TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> Generator;
	TSharedRef<const FVoxelOverlaySnapshotSet, ESPMode::ThreadSafe> Overlay;
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	TSharedPtr<const void, ESPMode::ThreadSafe> RegistrationLease;
	TSharedPtr<const FVoxelFineOverlayVersionProof, ESPMode::ThreadSafe> OverlayVersion;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InputLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> SamplingLease;
	TSharedPtr<FVoxelFineShardSamples, ESPMode::ThreadSafe> Samples;
	TArray<FIntVector> RequiredSections;
	int32 NextSection = 0;
	int32 WorldMinZ = 0;
	int32 WorldMaxZ = 0;
	bool bComplete = false;
	bool bInitialized = false;
};

struct WHFRAMEWORK_API FVoxelFineMeshEstimate
{
	uint64 SampleBytes = 0;
	uint64 MaximumVertices = 0;
	uint64 MaximumIndices = 0;
	uint64 MeshBytes = 0;
	uint64 SliceBytes = 0;
	uint64 MaskBytes = 0;
	uint64 MetadataBytes = 0;
	uint64 GrowthOverlapBytes = 0;
	uint64 ResultCapacityBytes = 0;
	uint64 WorkingBytes = 0;
	uint64 AdditionalPeakBytes = 0;
};

struct WHFRAMEWORK_API FVoxelFineMeshPiece
{
	FVoxelFineShardKey Key;
	TSharedPtr<const FVoxelSectionMeshResult, ESPMode::ThreadSafe> Mesh;
};

// 网格释放完整体采样后，边界事务仍需读取同一内容版本的两侧精确样本。
struct WHFRAMEWORK_API FVoxelFineAtomBoundary
{
	FIntVector Section = FIntVector::ZeroValue;
	TArray<uint32> Owned[6];
	TArray<uint32> Halo[6];
};

struct WHFRAMEWORK_API FVoxelFineBoundarySnapshot
{
	FVoxelFineMeshInput Input;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	TArray<uint32> Owned[6];
	TArray<uint32> Halo[6];
	TArray<FVoxelFineAtomBoundary> Atoms;
	bool Validate() const;
	uint64 GetAllocatedBytes() const;
	bool SampleFace(uint8 InFace, bool bInHalo, const FIntVector& InWorldCell, FVoxelBlockState& OutState) const;
	bool SampleAtomFace(const FIntVector& InSection, uint8 InFace, bool bInHalo,
		const FIntVector& InWorldCell, FVoxelBlockState& OutState) const;
};

struct FVoxelFineShardGeometry;

// 接口只持有其实际采样的边界与版本证明，不钉住几何正文。
struct WHFRAMEWORK_API FVoxelFineBoundarySource
{
	FVoxelFineBoundarySource() = default;
	explicit FVoxelFineBoundarySource(const FVoxelFineShardGeometry& InGeometry);
	bool Matches(const FVoxelFineShardGeometry& InGeometry) const;
	bool operator==(const FVoxelFineBoundarySource& InOther) const;
	bool operator!=(const FVoxelFineBoundarySource& InOther) const { return !(*this == InOther); }
	TSharedPtr<const FVoxelFineBoundarySnapshot, ESPMode::ThreadSafe> Boundary;
	TSharedPtr<const FVoxelFineOverlayVersionProof, ESPMode::ThreadSafe> OverlayVersion;
	FGuid GeometryIdentity;
};

struct WHFRAMEWORK_API FVoxelFineShardGeometry final : public FVoxelTaskCustomPayload
{
	FVoxelFineBuildAttempt Attempt;
	FGuid GeometryIdentity;
	TSharedPtr<const void, ESPMode::ThreadSafe> RegistrationLease;
	TSharedPtr<const FVoxelFineOverlayVersionProof, ESPMode::ThreadSafe> OverlayVersion;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
	TSharedPtr<const FVoxelFineBoundarySnapshot, ESPMode::ThreadSafe> Boundary;
	TArray<FVoxelFineMeshPiece> Pieces;
	uint64 PeakTemporaryBytes = 0;
	uint64 BuildMicrosteps = 0;
	uint64 VertexCount = 0;
	uint64 TriangleCount = 0;
	uint64 MeshCapacityBytes = 0;
	bool bKnownEmpty = false;
	bool bCoverageValidated = false;
	// 仅表示全部实体租约已进入普通 Resident；不参与内容或呈现证据。
	mutable TAtomic<bool> bResidentResourcesReady{false};
	virtual uint64 GetAllocatedBytes() const override;
	virtual uint64 GetResourceAccountedBytes() const override;
	bool TransferResources(EVoxelTerrainResourcePool InPool) const;
};

enum class EVoxelFineCapacityReason : uint8
{
	None = 0,
	WorkingTokenUnavailable,
	ResultTokenUnavailable,
	CapacityUnachievable
};

// 以下接口由生产规约D12-M按单一状态机实现，不能继续使用旧Meshing函数。
class WHFRAMEWORK_API FVoxelFineShardMeshing
{
public:
	FVoxelFineShardMeshing(TSharedRef<const FVoxelFineShardSamples, ESPMode::ThreadSafe> InSamples,
		TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> InRegistry,
		TSharedRef<const FVoxelShapeRegistry, ESPMode::ThreadSafe> InShapes,
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer, double InTextureRepeatsPerCell = 1.0);
	bool Initialize(uint64 InMaximumPieceBytes, uint64 InMaximumTotalGeometryBytes, FString& OutError);
	EVoxelFineBuildProgress Advance(const TAtomic<bool>& InCancel, FString& OutError);
	// InCapacityVersion 是原 Consumer 排除自身退款后的外部释放版本。
	bool ResumeCapacity(uint64 InCapacityVersion, FString& OutError);
	TSharedPtr<const FVoxelFineShardGeometry, ESPMode::ThreadSafe> TakeResult();
	uint64 AllocatedBytes() const;
	bool IsCapacityResumable() const;
	EVoxelFineCapacityReason GetCapacityReason() const;
	static FVoxelFineMeshEstimate MinimumPieceMemory(int32 InSideCells, int32 InPrimitiveBound);
	static bool Slice(const FVoxelFineShardSamples& InSource, const FVoxelFineShardKey& InWindow,
		FVoxelSectionSnapshot& OutSnapshot, FString& OutError);

private:
	void VisitResourceSources(FVoxelGenerationRequiredSourceScope::FVisit InVisit) const;
	void Abort();
	bool SplitPending(FString& OutError);
	bool ReserveMetadata(int32 InPendingCount, int32 InPieceCount, FString& OutError);
	EVoxelFineBuildProgress Block(EVoxelFineCapacityReason InReason, uint64 InWorkingBytes, uint64 InResultBytes);
	TSharedPtr<const FVoxelFineShardSamples, ESPMode::ThreadSafe> Samples;
	TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry;
	TSharedRef<const FVoxelShapeRegistry, ESPMode::ThreadSafe> Shapes;
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> PendingLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CapacityWorkingLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CapacityResultLease;
	TArray<FVoxelFineShardKey> Pending;
	TSharedPtr<FVoxelFineShardGeometry, ESPMode::ThreadSafe> Geometry;
	uint64 MaximumPieceBytes = 0;
	uint64 MaximumTotalGeometryBytes = 0;
	uint64 GeometryBytes = 0;
	uint64 CapacityVersion = 0;
	uint64 CompletedVolume = 0;
	int32 BlockedPendingCount = 0;
	int32 BlockedPieceCount = 0;
	double TextureRepeatsPerCell = 1.0;
	EVoxelFineBuildProgress State = EVoxelFineBuildProgress::None;
	EVoxelFineCapacityReason CapacityReason = EVoxelFineCapacityReason::None;
	uint64 RequiredWorkingBytes = 0;
	uint64 RequiredResultBytes = 0;
	bool bCapacityResumable = false;
	bool bCoverageValidated = false;
};
