#pragma once

#include "Ability/Item/AbilityTradableItemDataBase.h"

#include "AbilityPropDataBase.generated.h"

class AAbilityPropBase;

UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityPropDataBase : public UAbilityTradableItemDataBase
{
	GENERATED_BODY()

public:
	UAbilityPropDataBase();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AAbilityPropBase> PropClass;
};
