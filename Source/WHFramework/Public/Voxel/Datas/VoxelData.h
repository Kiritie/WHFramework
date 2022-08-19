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
	virtual void GetDefaultMeshData(const FVoxelItem& InVoxelItem, EVoxelMeshNature InMeshNature, FVector& OutMeshScale, FVector& OutMeshOffset) const;

	virtual void GetCustomMeshData(const FVoxelItem& InVoxelItem, EVoxelMeshNature InMeshNature, TArray<FVector>& OutMeshVertices, TArray<FVector>& OutMeshNormals) const;

	bool HasPartData(FIndex InIndex) const;

	UVoxelData& GetPartData(FIndex InIndex);

	FORCEINLINE FVector GetRange(FRotator InRotation = FRotator::ZeroRotator) const
	{
		FVector Range = FVector::OneVector;
		if(PartType == EVoxelPartType::Main && PartDatas.Num() > 0)
		{
			FVector PartRange;
			for(auto Iter : PartDatas)
			{
				PartRange.X = FMath::Max(Iter.Key.X, PartRange.X);
				PartRange.Y = FMath::Max(Iter.Key.Y, PartRange.Y);
				PartRange.Z = FMath::Max(Iter.Key.Z, PartRange.Z);
			}
			Range += PartRange;
		}
		if(InRotation != FRotator::ZeroRotator)
		{
			Range = UMathBPLibrary::RotatorVector(InRotation, Range, true, true);
		}
		return Range;
	}

	FORCEINLINE FVector2D GetUVCorner(EDirection InFacing, FVector2D InUVSize) const
	{
		return GetUVCorner((int)InFacing, InUVSize);
	}

	FORCEINLINE FVector2D GetUVCorner(int InFaceIndex, FVector2D InUVSize) const
	{
		FVoxelMeshUVData uvData;
		if (MeshUVDatas.Num() > InFaceIndex)
		{
			uvData = MeshUVDatas[InFaceIndex];
		}
		return FVector2D(uvData.UVCorner.X	 * InUVSize.X, (1 / InUVSize.Y - uvData.UVCorner.Y - uvData.UVSpan.Y) * InUVSize.Y);
	}

	FORCEINLINE FVector2D GetUVSpan(EDirection InFacing, FVector2D InUVSize) const
	{
		return GetUVSpan((int)InFacing, InUVSize);
	}

	FORCEINLINE FVector2D GetUVSpan(int InFaceIndex, FVector2D InUVSize) const
	{
		FVoxelMeshUVData uvData;
		if (MeshUVDatas.Num() > InFaceIndex)
		{
			uvData = MeshUVDatas[InFaceIndex];
		}
		return FVector2D(uvData.UVSpan.X * InUVSize.X, uvData.UVSpan.Y * InUVSize.Y);
	}
};
