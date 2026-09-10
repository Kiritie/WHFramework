#pragma once

#include "Parameter/ParameterModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_OpenUserWidget.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWidgetParameterArray
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FParameter> Value;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventOpenUserWidget : public FEventBase
{
	GENERATED_BODY()

public:
	FEventOpenUserWidget() = default;

	FEventOpenUserWidget(TSubclassOf<class UUserWidgetBase> InWidgetClass, FName InWidgetName, FWidgetParameterArray InWidgetParams, bool InbInstant, bool InbForce)
		: WidgetClass(MoveTemp(InWidgetClass)), WidgetName(MoveTemp(InWidgetName)), WidgetParams(MoveTemp(InWidgetParams)), bInstant(MoveTemp(InbInstant)), bForce(MoveTemp(InbForce))
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UUserWidgetBase> WidgetClass = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditConditionHides, EditCondition = "WidgetClass == nullptr"))
	FName WidgetName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FWidgetParameterArray WidgetParams = {};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bInstant = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bForce = false;
};
