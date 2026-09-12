#pragma once

#include "Components/NamedSlot.h"
#include "GameplayTagContainer.h"

#include "WidgetMountSlot.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UWidgetMountSlot : public UNamedSlot
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WidgetMountSlot", meta = (Categories = "WidgetSlot"))
	FGameplayTag SlotTag;

public:
	UFUNCTION(BlueprintPure)
	FGameplayTag GetSlotTag() const
	{
		return SlotTag;
	}
};
