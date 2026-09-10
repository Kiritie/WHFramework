#pragma once

#include "Parameter/ParameterModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_UserWidgetCreated.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventUserWidgetCreated : public FEventBase
{
	GENERATED_BODY()

public:
	FEventUserWidgetCreated() = default;

	FEventUserWidgetCreated(class UUserWidgetBase* InUserWidget)
		: UserWidget(MoveTemp(InUserWidget))
	{
	}

	UPROPERTY(BlueprintReadWrite)
	class UUserWidgetBase* UserWidget = nullptr;
};
