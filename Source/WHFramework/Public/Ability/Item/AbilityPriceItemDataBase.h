#pragma once

#include "AbilityItemDataBase.h"

#include "AbilityPriceItemDataBase.generated.h"

class AAbilityPickUpBase;

UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityPriceItemDataBase : public UAbilityItemDataBase
{
	GENERATED_BODY()

public:
	UAbilityPriceItemDataBase();

public:
	virtual void ResetData_Implementation() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Price;
};
