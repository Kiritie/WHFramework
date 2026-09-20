#pragma once
#include "UObject/PrimaryAssetId.h"
#include "Voxel/VoxelAuthoringTypes.h"
#include "CoreMinimal.h"
#include "SaveGame/SaveGameModuleTypes.h"
#include "Voxel/Generation/VoxelGenerationTypes.h"
#include "VoxelModuleTypes.generated.h"

class UVoxelWorldGenerationProfile;
UENUM(BlueprintType)
enum class EVoxelWorldMode:uint8{None,Default,Preview,Prefab};
UENUM(BlueprintType)
enum class EVoxelWorldState:uint8{None,Initializing,Running,Closing,Failed};
USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelItem:public FSaveData
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FPrimaryAssetId VoxelAssetID;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="0",ClampMax="65535")) int32 State=0;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="1")) int32 Count=1;
    FVoxelItem()=default;
    FVoxelItem(const FPrimaryAssetId& ID,int32 S=0,int32 N=1):VoxelAssetID(ID),State(S),Count(N){}
    virtual bool IsValid()const override;
};
USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelHitResult
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) bool bHit=false;
    UPROPERTY(BlueprintReadOnly) bool bNeedsData=false;
    UPROPERTY(BlueprintReadOnly) FIntVector Index=FIntVector::ZeroValue;
    UPROPERTY(BlueprintReadOnly) FIntVector PlacementIndex=FIntVector::ZeroValue;
    UPROPERTY(BlueprintReadOnly) FVector Point=FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) FVector Normal=FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) FVoxelItem VoxelItem;
    bool IsValid()const{return bHit&&VoxelItem.IsValid();}
};
USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelPrefabCell
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FIntVector Offset=FIntVector::ZeroValue;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FVoxelItem Item;
};
USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelPrefabSaveData:public FSaveData
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TArray<FVoxelPrefabCell> Cells;
};
USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelWorldBasicSaveData:public FSaveData
{
    GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UVoxelWorldGenerationProfile> GenerationProfile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Seed = 1;
};
USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelWorldSaveData:public FVoxelWorldBasicSaveData
{
    GENERATED_BODY()
    UPROPERTY() TArray<uint8> ManifestBytes;
    FVoxelWorldSaveData()=default;
    explicit FVoxelWorldSaveData(const FVoxelWorldBasicSaveData& B):FVoxelWorldBasicSaveData(B){}
};
USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelModuleSaveData:public FVoxelWorldSaveData
{
    GENERATED_BODY()
    FVoxelModuleSaveData()=default;
    explicit FVoxelModuleSaveData(const FVoxelWorldBasicSaveData& B):FVoxelWorldSaveData(B){}
};
