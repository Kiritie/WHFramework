#pragma once

#include "Parameter/ParameterModuleTypes.h"
#include "Voxel/VoxelModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_VoxelDestroyed.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventVoxelDestroyed : public FEventBase
{
	GENERATED_BODY()

public:
	FEventVoxelDestroyed() = default;

	FEventVoxelDestroyed(FVoxelItem InVoxelItem, TScriptInterface<IVoxelAgentInterface> InVoxelAgent)
		: VoxelItem(MoveTemp(InVoxelItem)), VoxelAgent(MoveTemp(InVoxelAgent))
	{
	}

	UPROPERTY(BlueprintReadWrite)
	FVoxelItem VoxelItem = FVoxelItem();

	UPROPERTY(BlueprintReadWrite)
	TScriptInterface<IVoxelAgentInterface> VoxelAgent = nullptr;
};
