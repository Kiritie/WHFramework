#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "VoxelCollisionTypes.generated.h"

UENUM(BlueprintType)
enum class EGameTraceChannel : uint8
{
	None = 0,
	Chunk = (uint8)ECC_GameTraceChannel9,
	Voxel = (uint8)ECC_GameTraceChannel10,
};
