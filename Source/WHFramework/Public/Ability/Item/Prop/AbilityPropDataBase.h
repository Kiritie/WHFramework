#pragma once

#include "Ability/Item/AbilityItemDataBase.h"

#include "AbilityPropDataBase.generated.h"

class AAbilityPickUpProp;
class AAbilityPropBase;
UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityPropDataBase : public UAbilityItemDataBase
{
	GENERATED_BODY()

public:
	UAbilityPropDataBase();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AAbilityPropBase> PropClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AAbilityPickUpProp> PropPickUpClass;
};
