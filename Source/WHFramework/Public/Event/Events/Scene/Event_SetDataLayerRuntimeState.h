#pragma once

#include "Parameter/ParameterModuleTypes.h"
#include "WorldPartition/DataLayer/ActorDataLayer.h"
#include "Event/EventModuleTypes.h"

#include "Event_SetDataLayerRuntimeState.generated.h"

class UDataLayerAsset;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventSetDataLayerRuntimeState : public FEventBase
{
	GENERATED_BODY()

public:
	FEventSetDataLayerRuntimeState() = default;

	FEventSetDataLayerRuntimeState(UDataLayerAsset* InDataLayer, EDataLayerRuntimeState InState, bool InbRecursive)
		: DataLayer(MoveTemp(InDataLayer)), State(MoveTemp(InState)), bRecursive(MoveTemp(InbRecursive))
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UDataLayerAsset* DataLayer = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EDataLayerRuntimeState State = EDataLayerRuntimeState::Unloaded;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bRecursive = false;
};
