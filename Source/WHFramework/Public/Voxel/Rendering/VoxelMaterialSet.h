#pragma once
#include "Engine/DataAsset.h"
#include "Voxel/Rendering/VoxelRenderTypes.h"
#include "VoxelMaterialSet.generated.h"

class UTexture2DArray;
class UMaterialInterface;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelMaterialBank
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere)
	EVoxelRenderGroup Group = EVoxelRenderGroup::Opaque;

	UPROPERTY(VisibleAnywhere)
	int32 Bank = 0;

	UPROPERTY(VisibleAnywhere)
	int32 SliceCount = 0;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTexture2DArray> Textures = nullptr;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UMaterialInterface> Material = nullptr;
};

UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelMaterialSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere)
	int32 BakeVersion = 0;

	UPROPERTY(VisibleAnywhere)
	TArray<FVoxelMaterialBank> Banks;

	const FVoxelMaterialBank* FindBank(EVoxelRenderGroup Group, uint16 Bank) const;

	bool Validate(FString& Error) const;
};
