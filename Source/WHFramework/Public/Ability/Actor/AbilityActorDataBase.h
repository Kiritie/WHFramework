#pragma once

#include "Ability/Item/AbilityItemDataBase.h"
#include "Ability/AbilityModuleTypes.h"

#include "AbilityActorDataBase.generated.h"

class AAbilityActorBase;
UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityActorDataBase : public UAbilityItemDataBase
{
	GENERATED_BODY()

public:
	UAbilityActorDataBase();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AAbilityActorBase> Class;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UGameplayEffect> PEClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Radius;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HalfHeight;
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FInventorySaveData InventoryData;
};
