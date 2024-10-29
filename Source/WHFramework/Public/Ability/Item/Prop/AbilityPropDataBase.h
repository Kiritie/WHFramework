#pragma once

#include "Ability/Item/AbilityTransactionItemDataBase.h"

#include "AbilityPropDataBase.generated.h"

class AAbilityPropBase;

UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityPropDataBase : public UAbilityTransactionItemDataBase
{
	GENERATED_BODY()

public:
	UAbilityPropDataBase();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AAbilityPropBase> PropClass;
};
