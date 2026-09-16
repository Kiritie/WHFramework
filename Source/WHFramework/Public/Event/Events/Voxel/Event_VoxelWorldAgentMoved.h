#pragma once

#include "Input/InputModuleTypes.h"
#include "Parameter/ParameterModuleTypes.h"
#include "Voxel/VoxelModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_VoxelWorldAgentMoved.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventVoxelWorldAgentMoved : public FEventBase
{
	GENERATED_BODY()

public:
	FEventVoxelWorldAgentMoved() = default;

	explicit FEventVoxelWorldAgentMoved(FIntVector InAgentIndex)
	    : AgentIndex(MoveTemp(InAgentIndex))
	{
	}

	UPROPERTY(BlueprintReadWrite)
	FIntVector AgentIndex = FIntVector::ZeroValue;
};
