#pragma once

#include "Ability/Item/AbilityItemDataBase.h"

#include "AbilityVitalityDataBase.generated.h"

class AAbilityVitalityBase;
UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityVitalityDataBase : public UAbilityItemDataBase
{
	GENERATED_BODY()

public:
	UAbilityVitalityDataBase();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AAbilityVitalityBase> Class;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UGameplayEffect> PEClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Radius;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HalfHeight;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tags")
	FGameplayTag DeadTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tags")
	FGameplayTag DyingTag;
};
