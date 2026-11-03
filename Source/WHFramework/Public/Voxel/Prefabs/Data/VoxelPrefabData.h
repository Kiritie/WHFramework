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

    /** 局部体素格偏移；正 Z 将整座预制体向上移动。各分量必须为有限整数。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel|Prefab", meta=(ToolTip="局部体素格偏移；正 Z 将整座预制体向上移动。各分量必须为有限整数。"))
    FVector CenterOffset = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel|Prefab")
    FVoxelPrefabSaveData Data;

    /** One complete prefab encoded with a shared voxel-type palette. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Voxel|Prefab")
    TArray<FPrimaryAssetId> PackedPalette;

    UPROPERTY()
    TArray<uint8> PackedCells;

    bool SetPackedData(const FVoxelPrefabSaveData& Value, FString& Error);
    /** 原始单元，不应用资产的 CenterOffset。 */
    bool DecodeCells(FVoxelPrefabSaveData& OutData, FString& Error) const;
    /** 按配置偏移后用于预览或放置的局部单元；包含清空操作。 */
    bool DecodePlacementCells(FVoxelPrefabSaveData& OutData, FString& Error) const;
    bool GetPlacementOffset(FIntVector& OutOffset, FString& Error) const;

    /** 偏移后的局部编辑范围，包含清空单元；无效配置返回无效包围盒。 */
    FBox GetVoxelBounds() const;
    bool Validate(const FVoxelRegistrySnapshot& Registry, FString& Error) const;
    static bool ValidateCells(const FVoxelPrefabSaveData& Value,
        const FVoxelRegistrySnapshot& Registry, FString& Error);
};
