#pragma once

#include "Ability/Item/AbilityItemDataBase.h"
#include "Ability/AbilityModuleTypes.h"
#include "Ability/Effects/EffectBase.h"

#include "AbilityActorDataBase.generated.h"

class AAbilityActorBase;
class UInventoryProfile;
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
	TSubclassOf<UEffectBase> PEClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Radius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HalfHeight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UInventoryProfile> InventoryProfile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FAbilityItem> InitialInventoryItems;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FInventoryInitialPlacement> InitialInventoryPlacements;
};
