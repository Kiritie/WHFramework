#pragma once

#include "VoxelData.h"
#include "Voxel/VoxelModuleTypes.h"

#include "VoxelTorchData.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelTorchData : public UVoxelData
{
	GENERATED_BODY()

public:
	UVoxelTorchData();

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	UParticleSystem* EffectAsset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector EffectScale;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector EffectOffset;
};
