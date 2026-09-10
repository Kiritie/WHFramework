#pragma once

#include "Parameter/ParameterModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_SetDataLayerOwnerPlayer.generated.h"

class UDataLayerAsset;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventSetDataLayerOwnerPlayer : public FEventBase
{
	GENERATED_BODY()

public:
	FEventSetDataLayerOwnerPlayer() = default;

	FEventSetDataLayerOwnerPlayer(UDataLayerAsset* InDataLayer, int32 InPlayerIndex)
		: DataLayer(MoveTemp(InDataLayer)), PlayerIndex(MoveTemp(InPlayerIndex))
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UDataLayerAsset* DataLayer = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PlayerIndex = -1;
};
