#pragma once

#include "Ability/Item/ItemDataBase.h"

#include "AbilityPropDataBase.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityPropDataBase : public UItemDataBase
{
	GENERATED_BODY()

public:
	UAbilityPropDataBase();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UStaticMesh* PropMesh;
};
