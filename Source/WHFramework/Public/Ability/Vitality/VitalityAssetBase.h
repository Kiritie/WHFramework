#pragma once

#include "Asset/Primary/Item/ItemAssetBase.h"

#include "VitalityAssetBase.generated.h"

class AAbilityVitalityBase;
UCLASS(BlueprintType)
class WHFRAMEWORK_API UVitalityAssetBase : public UItemAssetBase
{
	GENERATED_BODY()

public:
	UVitalityAssetBase();

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
