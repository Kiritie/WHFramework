#pragma once

#include "Voxel/VoxelModuleTypes.h"
#include "Input/InputModuleTypes.h"

#include "VoxelAgentInterface.generated.h"

UINTERFACE()
class WHFRAMEWORK_API UVoxelAgentInterface : public UInterface
{
	GENERATED_BODY()
};

class WHFRAMEWORK_API IVoxelAgentInterface
{
	GENERATED_BODY()

public:
	virtual bool OnGenerateVoxel(const FVoxelHitResult& InVoxelHitResult);

	virtual bool OnDestroyVoxel(const FVoxelHitResult& InVoxelHitResult);

	virtual bool OnInteractVoxel(const FVoxelHitResult& InVoxelHitResult, EInputInteractAction InInteractAction);

public:
	virtual FVector GetAgentLocation() const = 0;

	virtual FPrimaryAssetId GetGenerateVoxelID() const = 0;

	virtual void SetGenerateVoxelID(const FPrimaryAssetId& InGenerateVoxelID) = 0;
};