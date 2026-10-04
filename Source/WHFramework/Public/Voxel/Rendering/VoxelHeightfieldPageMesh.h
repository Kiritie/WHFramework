#pragma once

#include "CoreMinimal.h"
#include "Voxel/Rendering/VoxelHeightfieldMesher.h"
#include "Voxel/Rendering/VoxelMacroTerrain.h"
#include "Voxel/Rendering/VoxelWaterView.h"

// Continues in the original scheduler; only the completed parent page is publishable.
class WHFRAMEWORK_API FVoxelHeightfieldPageMesh
{
public:
	bool Initialize(const FVoxelTerrainGeometry& InGeometry, FString& OutError,
		uint64 InRetainedBytes = 12ull * 1024ull * 1024ull, bool bInCountOnly = false);
	uint64 GetEstimatedMeshBytes() const
	{
		return EstimatedMeshBytes;
	}
	bool Advance(const FVoxelSurfaceTileData& InData, const FVoxelWaterSurfaceTileData& InWater,
		const FVoxelGenerationRuntimeConfig& InConfig, const FVoxelRegistrySnapshot& InRegistry,
		double InTextureStretch, FVoxelSectionMeshResult& OutMesh, bool& bOutComplete,
		FString& OutError, const TAtomic<bool>* InCancel);
	bool Advance(const FVoxelMacroTileData& InData, const FVoxelGenerationRuntimeConfig& InConfig,
		const FVoxelRegistrySnapshot& InRegistry, double InTextureStretch,
		FVoxelSectionMeshResult& OutMesh, bool& bOutComplete, FString& OutError, const TAtomic<bool>* InCancel);

private:
	bool Retain(FVoxelSectionMeshResult&& InPart, FString& OutError);
	FVoxelTerrainGeometry Geometry;
	TArray<FVoxelTerrainBuildShard> Shards;
	FVoxelSectionMeshResult Mesh;
	int32 NextShard = 0;
	uint64 RetainedBytes = 0;
	uint64 EstimatedMeshBytes = 0;
	bool bCountOnly = false;
};
