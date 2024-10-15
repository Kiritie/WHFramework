#pragma once

#include "AbilityItemDataBase.h"

#include "AbilityTradableItemDataBase.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityTradableItemDataBase : public UAbilityItemDataBase
{
	GENERATED_BODY()

public:
	UAbilityTradableItemDataBase();

public:
	virtual void ResetData_Implementation() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FAbilityItem> Prices;
};
