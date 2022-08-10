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
	virtual bool GetMeshDatas(const FVoxelItem& InVoxelItem, TArray<FVector>& OutMeshVertices, TArray<FVector>& OutMeshNormals, FRotator InRotation) const override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	USoundBase* OpenSound;
						
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	USoundBase* CloseSound;
};
