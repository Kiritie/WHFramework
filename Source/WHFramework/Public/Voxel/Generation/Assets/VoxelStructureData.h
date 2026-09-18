#pragma once
#include "CoreMinimal.h"
#include "Asset/Primary/PrimaryAssetBase.h"
#include "VoxelStructureData.generated.h"
class UVoxelPrefabData;
class UVoxelData;
class UVoxelDetailData;
UENUM()
enum class EVoxelStructureKind : uint8 { Castle, Dungeon, TownHouse };
USTRUCT()
struct WHFRAMEWORK_API FVoxelGridBox
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) FIntVector Min=FIntVector::ZeroValue;
    UPROPERTY(EditAnywhere) FIntVector Max=FIntVector(1,1,1);
};
USTRUCT()
struct WHFRAMEWORK_API FVoxelStaticBlockMapping
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) TSoftObjectPtr<UVoxelData> Source;
    UPROPERTY(EditAnywhere) TSoftObjectPtr<UVoxelData> SolidReplacement;
};
USTRUCT()
struct WHFRAMEWORK_API FVoxelStructureDetailSocket
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) TSoftObjectPtr<UVoxelDetailData> Detail;
    UPROPERTY(EditAnywhere) FIntVector SourceCorner=FIntVector::ZeroValue;
    UPROPERTY(EditAnywhere,meta=(ClampMin="0",ClampMax="3")) int32 Yaw=0;
};
UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelStructureData : public UPrimaryAssetBase
{
    GENERATED_BODY()
public:
    UVoxelStructureData();
    UPROPERTY(EditAnywhere) FName StableKey;
    UPROPERTY(EditAnywhere) EVoxelStructureKind Kind=EVoxelStructureKind::TownHouse;
    UPROPERTY(EditAnywhere) TSoftObjectPtr<UVoxelPrefabData> SourcePrefab;
    UPROPERTY(EditAnywhere,meta=(ClampMin="10",ClampMax="100")) int32 SourceCellCentimeters=100;
    UPROPERTY(EditAnywhere) int32 SourceGroundCellZ=0;
    UPROPERTY(EditAnywhere) FIntVector SourceEntranceCorner=FIntVector::ZeroValue;
    UPROPERTY(EditAnywhere) FVoxelGridBox SourceBounds;
    UPROPERTY(EditAnywhere) TArray<FVoxelGridBox> ClearVolumes;
    UPROPERTY(EditAnywhere) TArray<FVoxelStaticBlockMapping> StaticMappings;
    UPROPERTY(EditAnywhere) TArray<FVoxelStructureDetailSocket> Details;
    UPROPERTY(EditAnywhere) bool bBakeDoorsOpen=true;
};
