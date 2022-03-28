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
	virtual bool GenerateVoxel(const FVoxelHitResult& InVoxelHitResult, FItem& InItem) = 0;

	virtual bool DestroyVoxel(const FVoxelHitResult& InVoxelHitResult) = 0;

public:
	virtual AVoxelChunk* GetOwnerChunk() const = 0;

	virtual void SetOwnerChunk(AVoxelChunk* InOwnerChunk) = 0;

	virtual FItem& GetGeneratingVoxelItem() = 0;

	virtual FVoxelItem& GetSelectedVoxelItem() = 0;
};