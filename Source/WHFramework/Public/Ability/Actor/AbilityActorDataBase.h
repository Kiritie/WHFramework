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
	virtual void OnInitialize_Implementation() override;

	virtual void OnReset_Implementation() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AActor> Class;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UGameplayEffect> PEClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Radius;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HalfHeight;
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FInventorySaveData InventoryData;
};
