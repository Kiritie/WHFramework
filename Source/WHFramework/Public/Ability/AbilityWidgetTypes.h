#pragma once

#include "CoreMinimal.h"
#include "Widget/WidgetMountTypes.h"
#include "Ability/Inventory/InventoryItemTypes.h"

#include "AbilityWidgetTypes.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAbilityWidgetSpawnParameter : public FSubWidgetSpawnParameter
{
	GENERATED_BODY()

public:
	FAbilityWidgetSpawnParameter() = default;

	explicit FAbilityWidgetSpawnParameter(const FAbilityItem& InItem)
		: Item(InItem)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FAbilityItem Item;
};
