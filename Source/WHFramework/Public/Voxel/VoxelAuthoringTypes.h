#pragma once
#include "CoreMinimal.h"
#include "VoxelAuthoringTypes.generated.h"
// Shared authoring vocabulary still used by RPG assets. Not a runtime TypeId
// table.
UENUM(BlueprintType)
enum class EVoxelGenerateToolType : uint8
{
	None,
	Pickaxe,
	Axe,
	Shovel
};
UENUM(BlueprintType)
enum class EVoxelBiomeType : uint8
{
	None,
	Snow,
	Green,
	Dry,
	Stone,
	Desert,
	Ocean,
	River,
	Plains,
	Forest,
	Taiga,
	Savanna,
	Swamp,
	Mountains,
	Badlands,
	Hills
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
