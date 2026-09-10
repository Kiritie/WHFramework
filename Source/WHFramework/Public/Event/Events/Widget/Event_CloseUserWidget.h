#pragma once

#include "Event/EventModuleTypes.h"

#include "Event_CloseUserWidget.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventCloseUserWidget : public FEventBase
{
	GENERATED_BODY()

public:
	FEventCloseUserWidget() = default;

	FEventCloseUserWidget(TSubclassOf<class UUserWidgetBase> InWidgetClass, FName InWidgetName, bool InbInstant)
		: WidgetClass(MoveTemp(InWidgetClass)), WidgetName(MoveTemp(InWidgetName)), bInstant(MoveTemp(InbInstant))
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UUserWidgetBase> WidgetClass = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditConditionHides, EditCondition = "WidgetClass == nullptr"))
	FName WidgetName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bInstant = false;
};
