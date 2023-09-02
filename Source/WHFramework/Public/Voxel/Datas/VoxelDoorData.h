#pragma once

#include "VoxelData.h"
#include "Voxel/VoxelModuleTypes.h"

#include "VoxelDoorData.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelDoorData : public UVoxelData
{
	GENERATED_BODY()

public:
	UVoxelDoorData();

public:
	virtual void GetMeshData(const FVoxelItem& InVoxelItem, FVector& OutMeshScale, FVector& OutMeshOffset) const override;

	virtual void GetUVData(const FVoxelItem& InVoxelItem, int32 InFaceIndex, FVector2D InUVSize, FVector2D& OutUVCorner, FVector2D& OutUVSpan) const override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector OpenedMeshScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector OpenedMeshOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FVoxelMeshUVData> OpenedMeshUVDatas;
};
