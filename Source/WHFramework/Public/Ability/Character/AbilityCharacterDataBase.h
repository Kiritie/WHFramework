#pragma once

#include "Ability/Item/AbilityItemDataBase.h"

#include "AbilityCharacterDataBase.generated.h"

class AAbilityCharacterBase;
UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityCharacterDataBase : public UAbilityItemDataBase
{
	GENERATED_BODY()

public:
	UAbilityCharacterDataBase();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Range;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AAbilityCharacterBase> Class;

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
