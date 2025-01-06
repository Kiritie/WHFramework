#pragma once

#include "AbilityItemDataBase.h"

#include "AbilityTransItemDataBase.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityTransItemDataBase : public UAbilityItemDataBase
{
	GENERATED_BODY()

public:
	UAbilityTransItemDataBase();

public:
	virtual void OnReset_Implementation() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FAbilityItem> Prices;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FAbilityItem> Upgrades;
};
