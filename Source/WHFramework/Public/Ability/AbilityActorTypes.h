#pragma once

#include "CoreMinimal.h"
#include "ObjectPool/ObjectPoolModuleTypes.h"

#include "AbilityActorTypes.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAbilityActorSpawnParameter : public FWHActorSpawnParameter
{
	GENERATED_BODY()

public:
	FAbilityActorSpawnParameter() = default;

	FAbilityActorSpawnParameter(
		UObject* InWorldContext,
		const FTransform& InTransform,
		const FGuid& InActorID,
		const FPrimaryAssetId& InAssetID)
	{
		WorldContext = InWorldContext;
		Transform = InTransform;
		bOverrideActorID = true;
		ActorID = InActorID;
		AssetID = InAssetID;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FPrimaryAssetId AssetID;
};
