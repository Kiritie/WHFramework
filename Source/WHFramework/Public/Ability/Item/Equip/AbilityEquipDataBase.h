#pragma once

#include "Ability/Item/AbilityTransactionItemDataBase.h"

#include "AbilityEquipDataBase.generated.h"

class AAbilityEquipBase;

UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityEquipDataBase : public UAbilityTransactionItemDataBase
{
	GENERATED_BODY()

public:
	UAbilityEquipDataBase();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AAbilityEquipBase> EquipClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EEquipRarity EquipRarity;
};
