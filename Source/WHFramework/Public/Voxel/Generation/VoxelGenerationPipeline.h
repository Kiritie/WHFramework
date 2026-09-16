#pragma once
#include "CoreMinimal.h"
#include "Voxel/Chunks/VoxelSectionStorage.h"
#include "Voxel/Generation/VoxelGenerationContext.h"
#include "Voxel/Generation/VoxelTerrainStage.h"
#include <atomic>
class WHFRAMEWORK_API FVoxelGenerationPipeline
{
public:
	explicit FVoxelGenerationPipeline(const FVoxelGenerationRuntimeConfig& In)
	    : Config(In)
	    , Terrain(In.Settings)
	{
	}
	bool GenerateSection(const FVoxelSectionKey& Key, FVoxelSectionStorage& Out, const std::atomic_bool* Cancel = nullptr) const;
	FVoxelBlockState SampleBaseBlock(const FIntVector& Position) const;
	FVoxelColumnSample SampleColumn(int32 X, int32 Y) const
	{
		return Terrain.SampleColumn(X, Y);
	}
	uint64 BuildHandshakeSignature() const;
	const FVoxelGenerationRuntimeConfig& GetConfig() const
	{
		return Config;
	}

private:
	FVoxelGenerationRuntimeConfig Config;
	FVoxelTerrainStage Terrain;
};
