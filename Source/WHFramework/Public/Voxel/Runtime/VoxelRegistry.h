#pragma once
#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"
#include "Voxel/Generation/VoxelGenerationContext.h"
#include "Voxel/Runtime/VoxelRuntimeDefinition.h"
class UVoxelData;
struct WHFRAMEWORK_API FVoxelRegistrySnapshot
{
	TArray<FVoxelRuntimeDefinition> Definitions;
	TMap<FName, uint16> Names;
	TMap<FPrimaryAssetId, uint16> Assets;
	uint64 Hash = 0;
	const FVoxelRuntimeDefinition* Find(uint16 Type) const;
	const FVoxelRuntimeDefinition* Find(FName Name) const;
	const FVoxelRuntimeDefinition* Find(const FPrimaryAssetId& Asset) const;
	bool IsValid(FVoxelBlockState S) const;
	bool BuildGenerationConfig(const FVoxelGenerationSettings& Settings, FVoxelGenerationRuntimeConfig& Out, FString& Error) const;
};
class WHFRAMEWORK_API FVoxelRegistry
{
public:
	bool Build(const TArray<UVoxelData*>& Assets, bool bRendering, FString& Error);
	TSharedPtr<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> GetSnapshot() const
	{
		return Published;
	}
	void Reset()
	{
		Published.Reset();
	}

private:
	TSharedPtr<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Published;
};
