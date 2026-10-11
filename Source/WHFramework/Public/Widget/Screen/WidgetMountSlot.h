#pragma once

#include "Components/NamedSlot.h"
#include "GameplayTagContainer.h"

#include "WidgetMountSlot.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UWidgetMountSlot : public UNamedSlot
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WidgetMountSlot", meta = (Categories = "Widget.Slot"))
	FGameplayTag SlotTag;

public:
	// Configure native layouts before the owning screen builds its mount-slot cache.
	void SetSlotTag(const FGameplayTag& InSlotTag)
	{
		SlotTag = InSlotTag;
	}

	UFUNCTION(BlueprintPure)
	FGameplayTag GetSlotTag() const
	{
		return SlotTag;
	}
};
