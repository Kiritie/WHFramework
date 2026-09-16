#pragma once
#include "Asset/Primary/PrimaryAssetBase.h"
#include "Voxel/VoxelModuleTypes.h"
#include "VoxelPrefabData.generated.h"

struct FVoxelRegistrySnapshot;

namespace VoxelPrefab
{
constexpr int32 MaxCellCount = 65536;
constexpr int32 MaxSectionCount = 32;
}

/** Finite, explicitly positioned block definitions. Never a second live world.
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelPrefabData : public UPrimaryAssetBase
{
	GENERATED_BODY()
public:
	UVoxelPrefabData();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Prefab")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Prefab")
	FVoxelPrefabSaveData Data;

	FBox GetVoxelBounds() const;
	bool Validate(const FVoxelRegistrySnapshot& Registry, FString& Error) const;
	static bool ValidateCells(const FVoxelPrefabSaveData& Value, const FVoxelRegistrySnapshot& Registry, FString& Error);
};
