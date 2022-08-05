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
	EVoxelPartType PartType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "Part == EVoxelPart::Main"))
	TMap<FIndex, UVoxelData*> PartDatas;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "Part == EVoxelPart::Part"))
	FIndex PartIndex;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "Part == EVoxelPart::Part"))
	UVoxelData* MainData;

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
	USoundBase* DestroySound;

public:
	FORCEINLINE FVector GetRange(FRotator InRotation = FRotator::ZeroRotator, FVector InScale = FVector::OneVector) const
	{
		FVector Range = FVector::OneVector;
		if(PartDatas.Num() > 0)
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
		Range = InRotation.RotateVector(Range * InScale);
		Range.X = FMath::Abs(Range.X);
		Range.Y = FMath::Abs(Range.Y);
		Range.Z = FMath::Abs(Range.Z);
		return Range;
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

	bool HasPartData(FIndex InIndex) const;

	UVoxelData& GetPartData(FIndex InIndex);

	bool GetMeshDatas(TArray<FVector>& OutMeshVertices, TArray<FVector>& OutMeshNormals, FRotator InRotation = FRotator::ZeroRotator, FVector InScale = FVector::OneVector) const;
};
