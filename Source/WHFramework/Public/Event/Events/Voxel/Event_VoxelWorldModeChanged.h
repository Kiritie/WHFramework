#pragma once

#include "Input/InputModuleTypes.h"
#include "Parameter/ParameterModuleTypes.h"
#include "Voxel/VoxelModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_VoxelWorldModeChanged.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventVoxelWorldModeChanged : public FEventBase
{
	GENERATED_BODY()

public:
	FEventVoxelWorldModeChanged() = default;

	FEventVoxelWorldModeChanged(EVoxelWorldMode InWorldMode)
		: WorldMode(MoveTemp(InWorldMode))
	{
	}

	UPROPERTY(BlueprintReadWrite)
	EVoxelWorldMode WorldMode = EVoxelWorldMode::None;
};
