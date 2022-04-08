#pragma once

#include "Ability/Item/ItemDataBase.h"

#include "VitalityDataBase.generated.h"

class AAbilityVitalityBase;
UCLASS(BlueprintType)
class WHFRAMEWORK_API UVitalityDataBase : public UItemDataBase
{
	GENERATED_BODY()

public:
	UVitalityDataBase();

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
	TSubclassOf<AAbilityVitalityBase> Class;
};
