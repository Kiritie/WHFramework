#pragma once

#include "Parameter/ParameterModuleTypes.h"
#include "WorldPartition/DataLayer/ActorDataLayer.h"
#include "Event/EventModuleTypes.h"

#include "Event_SetWorldWidgetVisible.generated.h"

class UWorldWidgetBase;
class UDataLayerAsset;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventSetWorldWidgetVisible : public FEventBase
{
	GENERATED_BODY()

public:
	FEventSetWorldWidgetVisible() = default;

	FEventSetWorldWidgetVisible(TSubclassOf<UWorldWidgetBase> InWidgetClass, bool InbVisible)
		: WidgetClass(MoveTemp(InWidgetClass)), bVisible(MoveTemp(InbVisible))
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UWorldWidgetBase> WidgetClass = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bVisible = false;
};
