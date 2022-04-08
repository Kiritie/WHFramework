#pragma once

#include "Ability/Item/ItemDataBase.h"

#include "CharacterDataBase.generated.h"

class AAbilityCharacterBase;
UCLASS(BlueprintType)
class WHFRAMEWORK_API UCharacterDataBase : public UItemDataBase
{
	GENERATED_BODY()

public:
	UCharacterDataBase();

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
