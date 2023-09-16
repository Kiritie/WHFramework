#pragma once

#include "SaveGame/SaveGameModuleTypes.h"
#include "Scene/SceneModuleTypes.h"
#include "Voxel/VoxelModuleTypes.h"

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
	virtual bool GenerateVoxel(const FVoxelHitResult& InVoxelHitResult);

	virtual bool DestroyVoxel(const FVoxelHitResult& InVoxelHitResult);

	virtual bool InteractVoxel(const FVoxelHitResult& InVoxelHitResult, EVoxelInteractType InInteractType);

public:
	virtual FVector GetWorldLocation() const = 0;

	virtual FPrimaryAssetId GetGenerateVoxelID() const = 0;

	virtual void SetGenerateVoxelID(const FPrimaryAssetId& InGenerateVoxelID) = 0;
};