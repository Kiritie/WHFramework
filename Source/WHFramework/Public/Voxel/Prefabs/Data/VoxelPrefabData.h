#pragma once
#include "Asset/Primary/PrimaryAssetBase.h"
#include "Voxel/VoxelModuleTypes.h"
#include "VoxelPrefabData.generated.h"

struct FVoxelRegistrySnapshot;

/** Finite, explicitly positioned block definitions. Never a second live world. */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelPrefabData : public UPrimaryAssetBase
{
    GENERATED_BODY()
public:
    UVoxelPrefabData();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel|Prefab")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel|Prefab")
    FVoxelPrefabSaveData Data;

    /** One complete prefab encoded with a shared voxel-type palette. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Voxel|Prefab")
    TArray<FPrimaryAssetId> PackedPalette;

    UPROPERTY()
    TArray<uint8> PackedCells;

    bool SetPackedData(const FVoxelPrefabSaveData& Value, FString& Error);
    bool DecodeCells(FVoxelPrefabSaveData& OutData, FString& Error) const;

    FBox GetVoxelBounds() const;
    bool Validate(const FVoxelRegistrySnapshot& Registry, FString& Error) const;
    static bool ValidateCells(const FVoxelPrefabSaveData& Value,
        const FVoxelRegistrySnapshot& Registry, FString& Error);
};
