#pragma once
#include "Ability/Item/AbilityItemDataBase.h"
#include "UObject/PrimaryAssetId.h"
#include "Voxel/Geometry/VoxelShapeTypes.h"
#include "Voxel/Rendering/VoxelRenderTypes.h"
#include "VoxelData.generated.h"
class USoundBase;
USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelBakedFaceRef
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere)
	int32 Bank = 0;
	UPROPERTY(VisibleAnywhere)
	int32 Layer = 0;
	UPROPERTY(VisibleAnywhere)
	int32 Frames = 1;
	UPROPERTY(VisibleAnywhere)
	int32 FPS = 0;
	bool ToRuntime(FVoxelRuntimeFaceRef& Out) const;
};
UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelData : public UAbilityItemDataBase
{
	GENERATED_BODY()
public:
	UVoxelData();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel")
	bool bRegisterBlock = true;
	virtual void OnInitialize_Implementation() override;
	virtual void OnReset_Implementation() override;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel")
	FName BlockName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel")
	EVoxelShapeKind Shape = EVoxelShapeKind::FullCube;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel")
	EVoxelRenderGroup RenderGroup = EVoxelRenderGroup::Opaque;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel")
	bool bSolid = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel")
	bool bOccludes = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel")
	bool bReplaceable = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel")
	bool bBreakable = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel", meta = (ClampMin = "0"))
	int32 BreakMilliseconds = 1000;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel")
	FPrimaryAssetId DropAssetID;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel", meta = (ClampMin = "0", ClampMax = "64"))
	int32 DropCount = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel", meta = (ClampMin = "0", ClampMax = "65535"))
	int32 EntityKind = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel", meta = (ClampMin = "0", ClampMax = "2"))
	int32 EntityVariant = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Rendering")
	FVoxelFaceMaterialSet FaceMaterials;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Rendering")
	FVoxelFaceMaterialSet UpperFaceMaterials;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Sound")
	TMap<FName, TSoftObjectPtr<USoundBase>> Sounds;
	UPROPERTY(VisibleAnywhere, Category = "Voxel|Baked")
	TArray<FVoxelBakedFaceRef> BakedFaces;
	UPROPERTY(VisibleAnywhere, Category = "Voxel|Baked")
	TArray<FVoxelBakedFaceRef> BakedUpperFaces;
	UPROPERTY(VisibleAnywhere, Category = "Voxel|Baked")
	int32 BakeVersion = 0;
	bool ValidateDefinition(bool bRequireRender, FString& OutError) const;
};
