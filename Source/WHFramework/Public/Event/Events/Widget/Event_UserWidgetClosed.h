#pragma once

#include "Event/EventModuleTypes.h"

#include "Event_UserWidgetClosed.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventUserWidgetClosed : public FEventBase
{
	GENERATED_BODY()

public:
	FEventUserWidgetClosed() = default;

	FEventUserWidgetClosed(class UUserWidgetBase* InUserWidget)
		: UserWidget(MoveTemp(InUserWidget))
	{
	}

	UPROPERTY(BlueprintReadWrite)
	class UUserWidgetBase* UserWidget = nullptr;
};
