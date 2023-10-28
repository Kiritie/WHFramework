#pragma once

#include "Ability/Item/AbilityItemDataBase.h"
#include "Ability/AbilityModuleTypes.h"

#include "AbilityPawnDataBase.generated.h"

class AAbilityPawnBase;
UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityPawnDataBase : public UAbilityItemDataBase
{
	GENERATED_BODY()

public:
	UAbilityPawnDataBase();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AAbilityPawnBase> Class;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UGameplayEffect> PEClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Radius;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HalfHeight;
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FInventorySaveData InventoryData;
};
