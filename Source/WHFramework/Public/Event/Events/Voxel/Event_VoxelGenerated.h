#pragma once

#include "Input/InputModuleTypes.h"
#include "Parameter/ParameterModuleTypes.h"
#include "Voxel/VoxelModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_VoxelGenerated.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventVoxelGenerated : public FEventBase
{
	GENERATED_BODY()

public:
	FEventVoxelGenerated() = default;

	FEventVoxelGenerated(FVoxelItem InVoxelItem, TScriptInterface<IVoxelAgentInterface> InVoxelAgent)
		: VoxelItem(MoveTemp(InVoxelItem)), VoxelAgent(MoveTemp(InVoxelAgent))
	{
	}

	UPROPERTY(BlueprintReadWrite)
	FVoxelItem VoxelItem = FVoxelItem();

	UPROPERTY(BlueprintReadWrite)
	TScriptInterface<IVoxelAgentInterface> VoxelAgent = nullptr;
};
