#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationRecipe.h"
#include "Voxel/Runtime/VoxelRegistry.h"

class UVoxelWorldGenerationProfile;

struct WHFRAMEWORK_API FVoxelGenerationRuntimeConfig
{
	TSharedPtr<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> Recipe;
	uint64 RegistryHash = 0;
	TArray<uint16> SymbolToRuntime;
	TArray<uint16> RuntimeToSymbol;
	FVoxelBlockState Air;
	FVoxelBlockState Stone;
	FVoxelBlockState Dirt;
	FVoxelBlockState Grass;
	FVoxelBlockState Sand;
	FVoxelBlockState Snow;
	FVoxelBlockState Water;
	FVoxelBlockState Lava;
	FVoxelBlockState Bedrock;
	FVoxelBlockState Road;

	bool IsValid() const;
	bool ToRuntime(uint32 InPackedSymbol, FVoxelBlockState& OutState) const;
	bool ToSymbol(FVoxelBlockState InState, uint32& OutPackedSymbol) const;
};

class WHFRAMEWORK_API FVoxelGenerationBinding
{
public:
	static bool Build(const UVoxelWorldGenerationProfile& InProfile, const FVoxelRegistrySnapshot& InRegistry,
		const FVoxelGenerationSettings& InSettings, int32 InCellCentimeters,
		FVoxelGenerationRuntimeConfig& OutConfig, FString& OutError);
};
