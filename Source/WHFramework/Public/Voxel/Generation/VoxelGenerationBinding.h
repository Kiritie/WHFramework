#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationRecipe.h"
#include "Voxel/Runtime/VoxelRegistry.h"

class UVoxelWorldGenerationProfile;
class FVoxelTerrainResources;
class FVoxelTerrainResourceLease;
struct FVoxelGenerationConsumerKey;

struct WHFRAMEWORK_API FVoxelGenerationRuntimeConfig
{
	// 冻结正文的 native backing 与控制器均由原 Fact 租约持有，复制强引用不再次授额。
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InputLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> RecipeInputLease;
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
	uint64 GetRequiredInputBytes(const FVoxelTerrainResources& InResources) const;
	int32 GetRequiredInputLeaseCount(const FVoxelTerrainResources& InResources) const;
};

class WHFRAMEWORK_API FVoxelGenerationBinding
{
public:
	static bool Build(const UVoxelWorldGenerationProfile& InProfile, const FVoxelRegistrySnapshot& InRegistry,
		const FVoxelGenerationSettings& InSettings, int32 InCellCentimeters,
		FVoxelGenerationRuntimeConfig& OutConfig, FString& OutError);
	// 作者解码正文只作输入；新不可变快照在全部 native 分配之前取得原世界额度。
	static bool Freeze(const FVoxelGenerationRuntimeConfig& InSource,
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer,
		TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe>& OutConfig,
		FString& OutError, uint64 InRequiredInputBytes = 0, int32 InRequiredLeaseCount = 0);
};
