#pragma once

#include "Asset/Primary/Item/ItemAssetBase.h"

#include "EquipAssetBase.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UEquipAssetBase : public UItemAssetBase
{
	GENERATED_BODY()

public:
	UEquipAssetBase();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UStaticMesh* EquipMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AAbilityEquipBase> EquipClass;
			
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UGameplayEffect> EffectClass;
};
