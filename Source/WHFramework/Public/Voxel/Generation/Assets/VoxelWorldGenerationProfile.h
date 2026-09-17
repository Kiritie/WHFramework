#pragma once
#include "CoreMinimal.h"
#include "Asset/Primary/PrimaryAssetBase.h"
#include "Voxel/Generation/VoxelGenerationContext.h"
#include "Voxel/Generation/Assets/VoxelStructureData.h"
#include "Voxel/Rendering/VoxelDetailData.h"
#include "VoxelWorldGenerationProfile.generated.h"
class UVoxelData;
UENUM()
enum class EVoxelTreeForm : uint8
{
	Broadleaf,
	Conifer
};
USTRUCT()
struct WHFRAMEWORK_API FVoxelTreeRuleData
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere)
    FName Key;
    UPROPERTY(EditAnywhere)
    EVoxelTreeForm Form=EVoxelTreeForm::Broadleaf;
    UPROPERTY(EditAnywhere)
    TArray<EVoxelBiomeId> Biomes;
    UPROPERTY(EditAnywhere)
    TSoftObjectPtr<UVoxelData> Trunk;
    UPROPERTY(EditAnywhere)
    TSoftObjectPtr<UVoxelData> Leaves;
    UPROPERTY(EditAnywhere)
    int32 MinHeight=96;
    UPROPERTY(EditAnywhere)
    int32 MaxHeight=160;
    UPROPERTY(EditAnywhere)
    int32 TrunkRadius=4;
    UPROPERTY(EditAnywhere)
    int32 CrownRadius=32;
    UPROPERTY(EditAnywhere)
    int32 ChancePermille=650;
    UPROPERTY(EditAnywhere)
    int32 MaxSlope=12;
};
USTRUCT()
struct WHFRAMEWORK_API FVoxelPlantRuleData
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere)
    FName Key;
    UPROPERTY(EditAnywhere)
    TArray<EVoxelBiomeId> Biomes;
    UPROPERTY(EditAnywhere)
    TSoftObjectPtr<UVoxelData> Block;
    UPROPERTY(EditAnywhere)
    int32 ChancePermille=100;
    UPROPERTY(EditAnywhere)
    int32 PatchPeriod=64;
};
USTRUCT()
struct WHFRAMEWORK_API FVoxelOreRuleData
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere)
    FName Key;
    UPROPERTY(EditAnywhere)
    TArray<EVoxelBiomeId> Biomes;
    UPROPERTY(EditAnywhere)
    TSoftObjectPtr<UVoxelData> Block;
    UPROPERTY(EditAnywhere)
    int32 MinZ=-480;
    UPROPERTY(EditAnywhere)
    int32 MaxZ=160;
    UPROPERTY(EditAnywhere)
    int32 Spacing=48;
    UPROPERTY(EditAnywhere)
    int32 Radius=3;
    UPROPERTY(EditAnywhere)
    int32 Length=16;
    UPROPERTY(EditAnywhere)
    int32 ChancePermille=600;
};
UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelWorldGenerationProfile : public UPrimaryAssetBase
{
    GENERATED_BODY()
public:
    UVoxelWorldGenerationProfile();
    UPROPERTY(EditAnywhere)
    int32 TargetCellCentimeters=25;
    UPROPERTY(EditAnywhere)
    FVoxelGenerationSettings Defaults;
    UPROPERTY(EditAnywhere)
    TSoftObjectPtr<UVoxelData> Stone;
    UPROPERTY(EditAnywhere)
    TSoftObjectPtr<UVoxelData> Dirt;
    UPROPERTY(EditAnywhere)
    TSoftObjectPtr<UVoxelData> Grass;
    UPROPERTY(EditAnywhere)
    TSoftObjectPtr<UVoxelData> Sand;
    UPROPERTY(EditAnywhere)
    TSoftObjectPtr<UVoxelData> Snow;
    UPROPERTY(EditAnywhere)
    TSoftObjectPtr<UVoxelData> Water;
    UPROPERTY(EditAnywhere)
    TSoftObjectPtr<UVoxelData> Lava;
    UPROPERTY(EditAnywhere)
    TSoftObjectPtr<UVoxelData> Bedrock;
    UPROPERTY(EditAnywhere)
    TSoftObjectPtr<UVoxelData> Road;
    UPROPERTY(EditAnywhere)
    int32 TreeSpacing=80;
    UPROPERTY(EditAnywhere)
    TArray<FVoxelTreeRuleData> Trees;
    UPROPERTY(EditAnywhere)
    TArray<FVoxelPlantRuleData> Plants;
    UPROPERTY(EditAnywhere)
    TArray<FVoxelOreRuleData> Ores;
    UPROPERTY(EditAnywhere)
    TArray<TSoftObjectPtr<UVoxelStructureData>> Structures;
    UPROPERTY(EditAnywhere)
    TArray<TSoftObjectPtr<UVoxelDetailData>> Details;
    UPROPERTY(VisibleAnywhere)
    TArray<uint8> CatalogBytes;
    UPROPERTY(VisibleAnywhere)
    uint64 CatalogHash=0;
    UPROPERTY(VisibleAnywhere)
    int32 CatalogBakeVersion=0;
    UPROPERTY(VisibleAnywhere)
    int32 BakedCellCentimeters=0;
};
