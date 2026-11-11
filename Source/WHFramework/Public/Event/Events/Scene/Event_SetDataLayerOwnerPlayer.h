#pragma once

#include "Parameter/ParameterModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_SetDataLayerOwnerPlayer.generated.h"

class UDataLayerAsset;
class APlayerController;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventSetDataLayerOwnerPlayer : public FEventBase
{
	GENERATED_BODY()

public:
	FEventSetDataLayerOwnerPlayer() = default;

	FEventSetDataLayerOwnerPlayer(UDataLayerAsset* InDataLayer, APlayerController* InPlayer)
		: DataLayer(InDataLayer), Player(InPlayer)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UDataLayerAsset* DataLayer = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<APlayerController> Player = nullptr;
};
