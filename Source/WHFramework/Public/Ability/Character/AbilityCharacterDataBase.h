#pragma once

#include "Ability/Item/AbilityItemDataBase.h"

#include "AbilityCharacterDataBase.generated.h"

class AAbilityCharacterBase;
class UGameplayEffect;
UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityCharacterDataBase : public UAbilityItemDataBase
{
	GENERATED_BODY()

public:
	UAbilityCharacterDataBase();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AAbilityCharacterBase> Class;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tags")
	FGameplayTag ActiveTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tags")
	FGameplayTag FallingTag;
				
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tags")
	FGameplayTag WalkingTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tags")
	FGameplayTag JumpingTag;
};
