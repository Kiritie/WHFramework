#pragma once

#include "Event/EventModuleTypes.h"
#include "GameplayTagContainer.h"

#include "Event_CloseUserWidget.generated.h"

class APlayerController;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventCloseUserWidget : public FEventBase
{
	GENERATED_BODY()

public:
	FEventCloseUserWidget() = default;

	FEventCloseUserWidget(APlayerController* InPlayer, FGameplayTag InWidgetTag, bool bInInstant)
		: Player(InPlayer), WidgetTag(InWidgetTag), bInstant(bInInstant)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<APlayerController> Player = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Categories = "Widget.Screen"))
	FGameplayTag WidgetTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bInstant = false;
};
