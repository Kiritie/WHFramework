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
	virtual const FVoxelMeshData& GetMeshData(const FVoxelItem& InVoxelItem) const override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector OpenedMeshScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector OpenedMeshOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FVoxelMeshUVData> OpenedMeshUVDatas;
};
