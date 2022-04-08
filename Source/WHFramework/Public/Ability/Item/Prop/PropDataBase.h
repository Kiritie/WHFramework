#pragma once

#include "Ability/Item/ItemDataBase.h"

#include "PropDataBase.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UPropDataBase : public UItemDataBase
{
	GENERATED_BODY()

public:
	UPropDataBase();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UStaticMesh* PropMesh;
};
