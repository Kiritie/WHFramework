#pragma once

#include "Ability/AbilityModuleTypes.h"
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
	virtual bool GenerateVoxel(FVoxelItem& InVoxelItem) = 0;

	virtual bool GenerateVoxel(FVoxelItem& InVoxelItem, const FVoxelHitResult& InVoxelHitResult) = 0;

	virtual bool DestroyVoxel(FVoxelItem& InVoxelItem) = 0;

	virtual bool DestroyVoxel(const FVoxelHitResult& InVoxelHitResult) = 0;

public:
	virtual FVector GetWorldLocation() const = 0;

	virtual FVoxelItem& GetGenerateVoxelItem() = 0;

	virtual void SetGenerateVoxelItem(FVoxelItem InGenerateVoxelItem) = 0;
};