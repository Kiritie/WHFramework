#pragma once

#include "CoreMinimal.h"
#include "Voxel/Rendering/VoxelCoverageIndex.h"
#include "Voxel/Authoring/VoxelViewProfile.h"
#include "Voxel/Rendering/VoxelViewTypes.h"
#include "Voxel/Streaming/VoxelFineSpatial.h"
#include "Voxel/Runtime/VoxelChangeIndex.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"

struct WHFRAMEWORK_API FVoxelAffectedRepresentationKeys
{
	FVoxelAffectedRepresentationKeys() = default;
	FVoxelAffectedRepresentationKeys(FVoxelAffectedRepresentationKeys&&) = default;
	FVoxelAffectedRepresentationKeys& operator=(FVoxelAffectedRepresentationKeys&& InOther);
	FVoxelAffectedRepresentationKeys(const FVoxelAffectedRepresentationKeys&) = delete;
	FVoxelAffectedRepresentationKeys& operator=(const FVoxelAffectedRepresentationKeys&) = delete;
	void Reset();
	// 原生容器先销毁，原Input授额最后释放。
	FVoxelChangeStorage Storage;
	TSet<FVoxelFineShardKey> FineShards;
	TSet<FVoxelViewKey> Proxy;
	TSet<FVoxelSurfaceTileKey> Surface;
	TSet<FVoxelMacroTileKey> Macro;
};

// 一次读锁内捕获持久编辑根；后续 Worker 查询不读取活动世界表，也不跨切点拼接版本。
struct WHFRAMEWORK_API FVoxelChangeHierarchySnapshot
{
	FVoxelCoverageIndex SectionChanges;
	uint64 ContentRevision = 0;
	int32 NaturalHorizontalInfluence = 0;
	int32 NaturalUpwardInfluence = 0;
	uint64 GetSectionRevision(const FIntVector& InSection) const;
	uint64 GetFineShardRevision(const FVoxelFineShardKey& InKey) const;
	uint64 GetGeometryRevision(const FVoxelTerrainGeometry& InGeometry) const;
};

struct FVoxelChangeHierarchy;

struct WHFRAMEWORK_API FVoxelPreparedChangeInvalidations
{
	FVoxelCoverageIndex SectionChanges;
	const FVoxelChangeHierarchy* Origin = nullptr;
	uint64 BeforeRevision = 0;
	int32 Count = 0;
};

struct WHFRAMEWORK_API FVoxelChangeHierarchy
{
public:
	~FVoxelChangeHierarchy();
	void BindResources(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer);
	EVoxelTerrainResourceAdmission GetLastResourceAdmission() const { return Storage.GetAdmission(); }
	FVoxelGenerationConsumerKey GetControlConsumer() const { return Storage.GetConsumer(); }
	bool RegisterGeometry(const FVoxelTerrainGeometry& InGeometry, FString& OutError, bool* OutChanged = nullptr);
	bool RegisterFineShard(const FVoxelFineShardKey& InKey, FString& OutError, bool* OutAdded = nullptr);
	bool ReleaseFineShard(const FVoxelFineShardKey& InKey, FString& OutError);
	bool PinFineShard(const FVoxelFineShardKey& InKey,
		TSharedPtr<const void, ESPMode::ThreadSafe>& OutToken, FString& OutError);
	static void VisitFineRegistrationSources(const TSharedPtr<const void, ESPMode::ThreadSafe>& InToken,
		const FVoxelTerrainResources& InResources, FVoxelGenerationRequiredSourceScope::FVisit InVisit);
	int32 AdvanceFineReleases(int32 InMaximum = 8);
	bool HasPendingFineReleases() const;
	// 读锁内共享不可变COW根；解锁后调用访问回调。
	void VisitAffectedFineShards(const FIntVector& InSection, TFunctionRef<void(const FVoxelFineShardKey&)> InVisit) const;
	void VisitAffectedKeys(const FIntVector& InSection, TFunctionRef<void(const FVoxelTerrainSpatialKey&)> InVisit) const;
	bool GatherAffectedFineShards(const FIntVector& InSection, FVoxelAffectedRepresentationKeys& OutKeys, FString& OutError) const;
	bool GatherAffectedKeys(const FIntVector& InSection, FVoxelAffectedRepresentationKeys& OutKeys, FString& OutError) const;
	// Prepare/Commit使用同一稳定Section来源；其间不可重入修改Hierarchy。
	bool PrepareInvalidations(int32 InCount, TFunctionRef<FIntVector(int32)> InSection,
		FVoxelPreparedChangeInvalidations& OutPrepared, FString& OutError);
	bool CommitInvalidations(FVoxelPreparedChangeInvalidations&& InPrepared,
		TFunctionRef<FIntVector(int32)> InSection, FString& OutError);
	bool InvalidateSection(const FIntVector& InSection, uint64& OutRevision, FString& OutError);
#if WITH_DEV_AUTOMATION_TESTS
	bool RegisterGeometry(const FVoxelTerrainGeometry& InGeometry);
	bool RegisterFineShard(const FVoxelFineShardKey& InKey);
	void ReleaseFineShard(const FVoxelFineShardKey& InKey);
	void ReleaseGeometry(const FVoxelTerrainSpatialKey& InKey);
	TSet<FVoxelFineShardKey> GatherAffectedFineShards(const FIntVector& InSection) const;
	FVoxelAffectedRepresentationKeys GatherAffectedKeys(const FIntVector& InSection) const;
	uint64 InvalidateSection(const FIntVector& InSection);
	void SetVoxelProxyRevision(const FVoxelViewKey& InKey, uint64 InRevision);
	void SetSurfaceRevision(const FVoxelSurfaceTileKey& InKey, uint64 InRevision);
	void SetMacroRevision(const FVoxelMacroTileKey& InKey, uint64 InRevision);
#endif
	uint64 GetFineShardRevision(const FVoxelFineShardKey& InKey) const;
	bool ReleaseGeometry(const FVoxelTerrainSpatialKey& InKey, FString& OutError);
	bool CanReleaseGeometryWithoutAllocation(const FVoxelTerrainSpatialKey& InKey) const;
	void SetTopology(const FVoxelLodTopologySettings& InLod);
	void SetNaturalInfluence(int32 InHorizontalCells, int32 InUpwardCells);
	uint64 GetSectionRevision(const FIntVector& InSection) const;
	uint64 GetContentRevision() const;
	FVoxelChangeHierarchySnapshot CaptureVersionSnapshot() const;
	bool IsVersionSnapshotCurrent(const FVoxelChangeHierarchySnapshot& InSnapshot,
		const FVoxelGenerationBounds& InBounds, int32 InHaloStep = 1) const;
	uint64 GetVoxelProxyRevision(const FVoxelViewKey& InProxyKey) const;
	uint64 GetSurfaceRevision(const FVoxelSurfaceTileKey& InSurfaceTile) const;
	uint64 GetMacroRevision(const FVoxelMacroTileKey& InMacroTile) const;
	bool SetVoxelProxyRevision(const FVoxelViewKey& InProxyKey, uint64 InRevision, FString& OutError);
	bool SetSurfaceRevision(const FVoxelSurfaceTileKey& InSurfaceTile, uint64 InRevision, FString& OutError);
	bool SetMacroRevision(const FVoxelMacroTileKey& InMacroTile, uint64 InRevision, FString& OutError);
	bool AffectsVoxelProxy(const FVoxelViewKey& InKey, const FIntVector& InSection) const;
	bool AffectsSurface(const FVoxelSurfaceTileKey& InKey, const FIntVector& InSection) const;
	bool AffectsMacro(const FVoxelMacroTileKey& InKey, const FIntVector& InSection) const;
	void ReleaseSection(const FIntVector& InSection);
	void Reset();

private:
	bool PrepareStorageRequirements(FString& OutError) const;
	bool PrepareCoverageRequirements(FVoxelCoverageIndex& InCandidate, FString& OutError,
		const FVoxelCoverageIndex* InRetainedOutput = nullptr) const;
	bool MeasureNativeInput(uint64& OutBytes, int32& OutCount, const FVoxelCoverageIndex* InCandidate,
		const FVoxelCoverageIndex* InRetainedOutput, FString& OutError) const;
	bool AffectsGeometry(const FVoxelTerrainSpatialKey& InKey, const FIntVector& InSection) const;
	uint64 CaptureGeometryRevision(const FVoxelTerrainGeometry& InGeometry, int32 InRetainedHaloStep) const;
	bool GatherAffectedLocked(const FIntVector& InSection, bool bGeometry,
		FVoxelAffectedRepresentationKeys& OutKeys, FString& OutError) const;
	bool AffectsFineShard(const FVoxelFineShardKey& InKey, const FIntVector& InSection) const;
	struct FFineReleaseState;
	struct FFineReleaseNode;
	struct FFineNodeDeleter;
	TSharedPtr<FFineReleaseState, ESPMode::ThreadSafe> FineReleaseState;
	mutable FVoxelChangeStorage Storage;
	FVoxelCoverageIndex GeometryIndex;
	FVoxelCoverageIndex SectionChangeIndex;
	FVoxelCoverageIndex FineShardIndex;
	TMap<FVoxelFineShardKey, uint64> FineShardRevisions;
	TMap<FVoxelFineShardKey, int32> FineShardReferences;
	TMap<FVoxelTerrainSpatialKey, int32> RetainedHaloSteps;
	TMap<int32, int32> HaloStepCounts;
	int32 MaximumHaloStep = 1;
	mutable FRWLock Lock;
	uint64 RevisionSerial = 0;
	int32 NaturalHorizontalInfluence = 0;
	int32 NaturalUpwardInfluence = 0;
	TMap<FIntVector, uint64> SectionRevision;
	TMap<FVoxelViewKey, uint64> VoxelProxyRevision;
	TMap<FVoxelSurfaceTileKey, uint64> SurfaceRevision;
	TMap<FVoxelMacroTileKey, uint64> MacroRevision;
};
