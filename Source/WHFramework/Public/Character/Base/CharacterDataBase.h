#pragma once

#include "Asset/Primary/PrimaryAssetBase.h"

#include "CharacterDataBase.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UCharacterDataBase : public UPrimaryAssetBase
{
	GENERATED_BODY()

public:
	UCharacterDataBase();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<FName, class UAnimMontage*> AnimMontages;
};
