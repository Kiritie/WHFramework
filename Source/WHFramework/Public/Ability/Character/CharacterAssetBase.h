#pragma once

#include "Asset/Primary/Item/ItemAssetBase.h"

#include "CharacterAssetBase.generated.h"

class AAbilityCharacterBase;
UCLASS(BlueprintType)
class WHFRAMEWORK_API UCharacterAssetBase : public UItemAssetBase
{
	GENERATED_BODY()

public:
	UCharacterAssetBase();

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
