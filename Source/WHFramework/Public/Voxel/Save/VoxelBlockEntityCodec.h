#pragma once
#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"
#include "Voxel/Save/VoxelDeltaTypes.h"
struct WHFRAMEWORK_API FVoxelItemStack
{
	FPrimaryAssetId ID;
	int32 Count = 0;
	int32 Level = 0;
};
class WHFRAMEWORK_API FVoxelBlockEntityCodec
{
public:
	static bool MakeDefault(uint16 Kind, FVoxelBlockEntityState& Out, uint8 Variant = 0);
	static bool Validate(const FVoxelBlockEntityState& State);
	static bool EncodeContainer(const TArray<FVoxelItemStack>& Slots, FVoxelBlockEntityState& Out);
	static bool DecodeContainer(const FVoxelBlockEntityState& State, TArray<FVoxelItemStack>& Out);
	static bool IncrementCounter(const FVoxelBlockEntityState& In, FVoxelBlockEntityState& Out);
};
