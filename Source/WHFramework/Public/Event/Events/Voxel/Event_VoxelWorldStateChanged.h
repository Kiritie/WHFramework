#pragma once

#include "Input/InputModuleTypes.h"
#include "Parameter/ParameterModuleTypes.h"
#include "Voxel/VoxelModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_VoxelWorldStateChanged.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventVoxelWorldStateChanged : public FEventBase
{
	GENERATED_BODY()

public:
	FEventVoxelWorldStateChanged() = default;

	FEventVoxelWorldStateChanged(EVoxelWorldState InWorldState)
		: WorldState(MoveTemp(InWorldState))
	{
	}

	UPROPERTY(BlueprintReadWrite)
	EVoxelWorldState WorldState = EVoxelWorldState::None;
};
