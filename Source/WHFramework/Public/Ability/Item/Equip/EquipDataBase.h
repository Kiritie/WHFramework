#pragma once

#include "Ability/Item/ItemDataBase.h"

#include "EquipDataBase.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UEquipDataBase : public UItemDataBase
{
	GENERATED_BODY()

public:
	UEquipDataBase();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UStaticMesh* EquipMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AAbilityEquipBase> EquipClass;
			
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UGameplayEffect> EffectClass;
};
