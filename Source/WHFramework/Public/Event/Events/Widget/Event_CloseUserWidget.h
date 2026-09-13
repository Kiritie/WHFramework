#pragma once

#include "Event/EventModuleTypes.h"

#include "Event_CloseUserWidget.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventCloseUserWidget : public FEventBase
{
	GENERATED_BODY()

public:
	FEventCloseUserWidget() = default;

	FEventCloseUserWidget(FGameplayTag InWidgetTag, bool bInInstant)
		: WidgetTag(InWidgetTag), bInstant(bInInstant)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Categories = "Widget.Screen"))
	FGameplayTag WidgetTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bInstant = false;
};
