#pragma once

#include "Ability/Item/AbilityTradableItemDataBase.h"

#include "AbilityEquipDataBase.generated.h"

class AAbilityEquipBase;

UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityEquipDataBase : public UAbilityTradableItemDataBase
{
	GENERATED_BODY()

public:
	UAbilityEquipDataBase();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AAbilityEquipBase> EquipClass;
};
