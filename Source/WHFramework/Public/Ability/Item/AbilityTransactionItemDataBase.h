#pragma once

#include "AbilityItemDataBase.h"

#include "AbilityTransactionItemDataBase.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityTransactionItemDataBase : public UAbilityItemDataBase
{
	GENERATED_BODY()

public:
	UAbilityTransactionItemDataBase();

public:
	virtual void OnReset_Implementation() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FAbilityItem> Prices;
};
