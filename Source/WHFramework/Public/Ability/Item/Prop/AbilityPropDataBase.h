#pragma once

#include "Ability/Item/AbilityPriceItemDataBase.h"

#include "AbilityPropDataBase.generated.h"

class AAbilityPropBase;

UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityPropDataBase : public UAbilityPriceItemDataBase
{
	GENERATED_BODY()

public:
	UAbilityPropDataBase();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AAbilityPropBase> PropClass;
};
