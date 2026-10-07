#pragma once

#include "CoreMinimal.h"
#include "Voxel/Rendering/VoxelProxyBuilder.h"

class WHFRAMEWORK_API FVoxelProxyPageBuild
{
public:
	bool Initialize(const FVoxelTerrainGeometry& InGeometry, FString& OutError);
	bool Advance(const FVoxelVoxelProxyBuilder& InBuilder, const FVoxelOverlaySnapshotSet& InOverlays,
		FVoxelVoxelProxyData& OutData, bool& bOutComplete, FString& OutError, const TAtomic<bool>* InCancel);
	uint64 MaximumRetainedBytes() const;

private:
	FVoxelTerrainGeometry Geometry;
	TArray<FVoxelTerrainBuildShard> Shards;
	FVoxelVoxelProxyData Page;
	bool bAllocated = false;
	int32 NextShard = 0;
};
