#pragma once

#include "Ability/Item/AbilityItemDataBase.h"
#include "Math/MathBPLibrary.h"
#include "Voxel/VoxelModuleTypes.h"

#include "VoxelData.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelData : public UAbilityItemDataBase
{
	GENERATED_BODY()

public:
	UVoxelData();

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EVoxelType VoxelType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UVoxel> VoxelClass;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<AVoxelAuxiliary> AuxiliaryClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EVoxelTransparency Transparency;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EVoxelPartType PartType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "PartType == EVoxelPartType::Main"))
	TMap<FIndex, UVoxelData*> PartDatas;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "PartType == EVoxelPartType::Part"))
	FIndex PartIndex;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "PartType == EVoxelPartType::Part"))
	UVoxelData* MainData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EVoxelMeshType MeshType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector MeshScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector MeshOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "MeshType == EVoxelMeshType::Custom"))
	TArray<FVector> MeshVertices;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "MeshType == EVoxelMeshType::Custom"))
	TArray<FVector> MeshNormals;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FVoxelMeshUVData> MeshUVDatas;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	USoundBase* GenerateSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	USoundBase* DestroySound;

public:
	virtual void GetMeshData(const FVoxelItem& InVoxelItem, FVector& OutMeshScale, FVector& OutMeshOffset) const;

	virtual void GetMeshData(const FVoxelItem& InVoxelItem, TArray<FVector>& OutMeshVertices, TArray<FVector>& OutMeshNormals) const;

	virtual void GetUVData(const FVoxelItem& InVoxelItem, int32 InFaceIndex, FVector2D InUVSize, FVector2D& OutUVCorner, FVector2D& OutUVSpan) const;

	virtual bool HasPartData(FIndex InIndex) const;

	virtual UVoxelData& GetPartData(FIndex InIndex);

	virtual FVector GetRange(ERightAngle InAngle = ERightAngle::RA_0) const;
};
