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
	virtual void GetDefaultMeshData(const FVoxelItem& InVoxelItem, FVector& OutMeshScale, FVector& OutMeshOffset) const override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	USoundBase* OpenSound;
						
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	USoundBase* CloseSound;
};
