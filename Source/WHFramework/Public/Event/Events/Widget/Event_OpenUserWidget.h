#pragma once

#include "Parameter/ParameterModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_OpenUserWidget.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventOpenUserWidget : public FEventBase
{
	GENERATED_BODY()

public:
	FEventOpenUserWidget() = default;

	FEventOpenUserWidget(FGameplayTag InWidgetTag, const FParameter& InWidgetParam, bool bInInstant, bool bInForce, TSubclassOf<class UUserWidgetBase> InWidgetClassOverride = nullptr)
		: WidgetTag(InWidgetTag), WidgetClassOverride(InWidgetClassOverride), WidgetParam(InWidgetParam), bInstant(bInInstant), bForce(bInForce)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Categories = "Widget.Screen"))
	FGameplayTag WidgetTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UUserWidgetBase> WidgetClassOverride = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FParameter WidgetParam;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bInstant = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bForce = false;
};
