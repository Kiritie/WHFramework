#pragma once

#include "CoreMinimal.h"
#include "Voxel/Rendering/VoxelCoverageIndex.h"
#include "Voxel/Authoring/VoxelViewProfile.h"
#include "Voxel/Rendering/VoxelViewTypes.h"
#include "Voxel/Streaming/VoxelFineSpatial.h"

struct WHFRAMEWORK_API FVoxelAffectedRepresentationKeys
{
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

struct WHFRAMEWORK_API FVoxelChangeHierarchy
{
public:
	bool RegisterGeometry(const FVoxelTerrainGeometry& InGeometry);
	bool RegisterFineShard(const FVoxelFineShardKey& InKey);
	void ReleaseFineShard(const FVoxelFineShardKey& InKey);
	TSet<FVoxelFineShardKey> GatherAffectedFineShards(const FIntVector& InSection) const;
	uint64 GetFineShardRevision(const FVoxelFineShardKey& InKey) const;
	void ReleaseGeometry(const FVoxelTerrainSpatialKey& InKey);
	void SetTopology(const FVoxelLodTopologySettings& InLod);
	FVoxelAffectedRepresentationKeys GatherAffectedKeys(const FIntVector& InSection) const;
	void SetNaturalInfluence(int32 InHorizontalCells, int32 InUpwardCells);
	uint64 InvalidateSection(const FIntVector& InSection);
	uint64 GetSectionRevision(const FIntVector& InSection) const;
	uint64 GetContentRevision() const;
	FVoxelChangeHierarchySnapshot CaptureVersionSnapshot() const;
	bool IsVersionSnapshotCurrent(const FVoxelChangeHierarchySnapshot& InSnapshot,
		const FVoxelGenerationBounds& InBounds, int32 InHaloStep = 1) const;
	uint64 GetVoxelProxyRevision(const FVoxelViewKey& InProxyKey) const;
	uint64 GetSurfaceRevision(const FVoxelSurfaceTileKey& InSurfaceTile) const;
	uint64 GetMacroRevision(const FVoxelMacroTileKey& InMacroTile) const;
	void SetVoxelProxyRevision(const FVoxelViewKey& InProxyKey, uint64 InRevision);
	void SetSurfaceRevision(const FVoxelSurfaceTileKey& InSurfaceTile, uint64 InRevision);
	void SetMacroRevision(const FVoxelMacroTileKey& InMacroTile, uint64 InRevision);
	bool AffectsVoxelProxy(const FVoxelViewKey& InKey, const FIntVector& InSection) const;
	bool AffectsSurface(const FVoxelSurfaceTileKey& InKey, const FIntVector& InSection) const;
	bool AffectsMacro(const FVoxelMacroTileKey& InKey, const FIntVector& InSection) const;
	void ReleaseSection(const FIntVector& InSection);
	void Reset();

private:
	bool AffectsGeometry(const FVoxelTerrainSpatialKey& InKey, const FIntVector& InSection) const;
	uint64 CaptureGeometryRevision(const FVoxelTerrainGeometry& InGeometry, int32 InRetainedHaloStep) const;
	TSet<FVoxelFineShardKey> GatherAffectedFineShardsLocked(const FIntVector& InSection) const;
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
