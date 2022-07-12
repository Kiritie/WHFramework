#pragma once

#include "Ability/Item/AbilityItemDataBase.h"

#include "AbilityEquipDataBase.generated.h"

class AAbilityPickUpEquip;
UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityEquipDataBase : public UAbilityItemDataBase
{
	GENERATED_BODY()

public:
	UAbilityEquipDataBase();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AAbilityEquipBase> EquipClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AAbilityPickUpEquip> EquipPickUpClass;
};
