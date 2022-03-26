#pragma once

#include "Asset/Primary/Item/ItemAssetBase.h"

#include "PropAssetBase.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UPropAssetBase : public UItemAssetBase
{
	GENERATED_BODY()

public:
	UPropAssetBase();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UStaticMesh* PropMesh;
};
