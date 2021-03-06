#pragma once

#include "Ability/Item/AbilityItemDataBase.h"
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
	FVector Range;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Offset;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bCustomMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bCustomMesh == true"))
	TArray<FVector> MeshVertices;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bCustomMesh == true"))
	TArray<FVector> MeshNormals;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FVoxelMeshUVData> MeshUVDatas;
			
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	USoundBase* GenerateSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<USoundBase*> OperationSounds;

public:
	FORCEINLINE bool IsComplex() const
	{
		return GetCeilRange() != FVector::OneVector;
	}

	FORCEINLINE FVector GetCeilRange(FRotator InRotation = FRotator::ZeroRotator, FVector InScale = FVector::OneVector) const
	{
		FVector range = GetFinalRange(InRotation, InScale);
		range.X = FMath::CeilToFloat(range.X);
		range.Y = FMath::CeilToFloat(range.Y);
		range.Z = FMath::CeilToFloat(range.Z);
		return range;
	}

	FORCEINLINE FVector GetFinalRange(FRotator InRotation = FRotator::ZeroRotator, FVector InScale = FVector::OneVector) const
	{
		FVector range = InRotation.RotateVector(Range * InScale);
		range.X = FMath::Abs(range.X);
		range.Y = FMath::Abs(range.Y);
		range.Z = FMath::Abs(range.Z);
		return range;
	}

	FORCEINLINE FVector2D GetUVCorner(EVoxelFacing InFacing, FVector2D InUVSize) const
	{
		return GetUVCorner((int)InFacing, InUVSize);
	}

	FORCEINLINE FVector2D GetUVCorner(int InFaceIndex, FVector2D InUVSize) const
	{
		FVoxelMeshUVData uvData = FVoxelMeshUVData();
		if (MeshUVDatas.Num() > InFaceIndex)
			uvData = MeshUVDatas[InFaceIndex];
		return FVector2D(uvData.UVCorner.X	 * InUVSize.X, (1 / InUVSize.Y - uvData.UVCorner.Y - uvData.UVSpan.Y) * InUVSize.Y);
	}

	FORCEINLINE FVector2D GetUVSpan(EVoxelFacing InFacing, FVector2D InUVSize) const
	{
		return GetUVSpan((int)InFacing, InUVSize);
	}

	FORCEINLINE FVector2D GetUVSpan(int InFaceIndex, FVector2D InUVSize) const
	{
		FVoxelMeshUVData uvData = FVoxelMeshUVData();
		if (MeshUVDatas.Num() > InFaceIndex)
			uvData = MeshUVDatas[InFaceIndex];
		return FVector2D(uvData.UVSpan.X * InUVSize.X, uvData.UVSpan.Y * InUVSize.Y);
	}

	bool GetMeshDatas(TArray<FVector>& OutMeshVertices, TArray<FVector>& OutMeshNormals, FRotator InRotation = FRotator::ZeroRotator, FVector InScale = FVector::OneVector) const;
};
