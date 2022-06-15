#pragma once

#include "Ability/Item/ItemDataBase.h"

#include "AbilityEquipDataBase.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityEquipDataBase : public UItemDataBase
{
	GENERATED_BODY()

public:
	UAbilityEquipDataBase();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UStaticMesh* EquipMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AAbilityEquipBase> EquipClass;
			
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UGameplayEffect> EffectClass;
};
