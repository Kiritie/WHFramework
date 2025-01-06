#pragma once

#include "Ability/Item/AbilityTransItemDataBase.h"

#include "AbilityPropDataBase.generated.h"

class AAbilityPropBase;

UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityPropDataBase : public UAbilityTransItemDataBase
{
	GENERATED_BODY()

public:
	UAbilityPropDataBase();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AAbilityPropBase> PropClass;
};
