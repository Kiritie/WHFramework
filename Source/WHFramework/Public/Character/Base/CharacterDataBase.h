#pragma once

#include "Pawn/Base/PawnDataBase.h"

#include "CharacterDataBase.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UCharacterDataBase : public UPawnDataBase
{
	GENERATED_BODY()

public:
	UCharacterDataBase();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<FName, class UAnimMontage*> AnimMontages;
};
