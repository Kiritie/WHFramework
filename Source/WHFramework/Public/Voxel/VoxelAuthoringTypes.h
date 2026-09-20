#pragma once
#include "CoreMinimal.h"
#include "VoxelAuthoringTypes.generated.h"
UENUM(BlueprintType)
enum class EVoxelGenerateToolType : uint8
{
	None,
	Pickaxe,
	Axe,
	Shovel
};

UENUM(BlueprintType)
enum class EVoxelRegionType : uint8
{
	None,
	Wilderness,
	Plain,
	Mountain,
	Hills,
	River,
	Lake,
	Ocean,
	Town,
	Building,
	Desert,
	Icefield
};

UENUM(BlueprintType)
enum class EVoxelAreaType : uint8
{
	Continent,
	Town,
	Building
};
