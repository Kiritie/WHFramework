#pragma once
#include "CoreMinimal.h"
#include "Asset/Primary/PrimaryAssetBase.h"
#include "Voxel/Generation/Assets/VoxelStructureData.h"
#include "VoxelDetailData.generated.h"
class UStaticMesh;
class UMaterialInterface;
UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelDetailData : public UPrimaryAssetBase
{
    GENERATED_BODY()
public:
    UVoxelDetailData();
    UPROPERTY(EditAnywhere) FName StableKey;
    UPROPERTY(EditAnywhere) TSoftObjectPtr<UStaticMesh> Mesh;
    // Optional native mesh source: centimeters, triangle indices, one material. Editor-only conversion.
    UPROPERTY(EditAnywhere) TArray<FVector> SourceVerticesCm;
    UPROPERTY(EditAnywhere) TArray<int32> SourceTriangles;
    UPROPERTY(EditAnywhere) TSoftObjectPtr<UMaterialInterface> SourceMaterial;
    UPROPERTY(EditAnywhere) TArray<FBox> CollisionBoxesCm;
    UPROPERTY(EditAnywhere) FVoxelGridBox FootprintCells;
    UPROPERTY(EditAnywhere) bool bNearCollision=true;
    UPROPERTY(VisibleAnywhere) uint64 GeometryHash=0;
    UPROPERTY(VisibleAnywhere) uint64 MetadataHash=0;
    UPROPERTY(VisibleAnywhere) int32 BakeVersion=0;
    bool Validate(FString& Error) const;
    uint64 ComputeMetadataHash() const;
};
