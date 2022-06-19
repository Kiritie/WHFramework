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
	int32 EXP;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 BaseEXP;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 EXPFactor;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AAbilityCharacterBase> Class;
};
