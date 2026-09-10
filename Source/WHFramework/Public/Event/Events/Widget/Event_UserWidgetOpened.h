#pragma once

#include "Parameter/ParameterModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_UserWidgetOpened.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventUserWidgetOpened : public FEventBase
{
	GENERATED_BODY()

public:
	FEventUserWidgetOpened() = default;

	FEventUserWidgetOpened(class UUserWidgetBase* InUserWidget)
		: UserWidget(MoveTemp(InUserWidget))
	{
	}

	UPROPERTY(BlueprintReadWrite)
	class UUserWidgetBase* UserWidget = nullptr;
};
