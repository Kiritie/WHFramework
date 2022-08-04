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
	virtual bool GenerateVoxel(const FVoxelHitResult& InVoxelHitResult);

	virtual bool DestroyVoxel(const FVoxelHitResult& InVoxelHitResult);

public:
	virtual FVector GetWorldLocation() const = 0;

	virtual FVoxelItem& GetGenerateVoxelItem() = 0;

	virtual void SetGenerateVoxelItem(const FVoxelItem& InGenerateVoxelItem) = 0;
};