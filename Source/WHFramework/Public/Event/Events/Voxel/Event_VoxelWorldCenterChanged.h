#pragma once

#include "Input/InputModuleTypes.h"
#include "Parameter/ParameterModuleTypes.h"
#include "Voxel/VoxelModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_VoxelWorldCenterChanged.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventVoxelWorldCenterChanged : public FEventBase
{
	GENERATED_BODY()

public:
	FEventVoxelWorldCenterChanged() = default;

	FEventVoxelWorldCenterChanged(FIndex InWorldCenter)
		: WorldCenter(MoveTemp(InWorldCenter))
	{
	}

	UPROPERTY(BlueprintReadWrite)
	FIndex WorldCenter = FIndex::ZeroIndex;
};
