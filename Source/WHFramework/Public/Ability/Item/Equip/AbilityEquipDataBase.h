#pragma once

#include "Ability/Item/AbilityTransItemDataBase.h"

#include "AbilityEquipDataBase.generated.h"

class AAbilityEquipBase;

UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityEquipDataBase : public UAbilityTransItemDataBase
{
	GENERATED_BODY()

public:
	UAbilityEquipDataBase();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EEquipMode EquipMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EEquipRarity EquipRarity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AAbilityEquipBase> EquipClass;
};
