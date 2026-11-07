#pragma once

#include "CoreMinimal.h"
#include "Parameter/ParameterModuleTypes.h"

#include "SceneActorSaveTypes.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSceneActorSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FSceneActorSaveData()
	{
		ActorID = FGuid::NewGuid();
		SpawnTransform = FTransform::Identity;
	}

public:
	UPROPERTY(BlueprintReadWrite, meta = (IgnoreForMemberInitializationTest))
	FGuid ActorID;

	UPROPERTY()
	FTransform SpawnTransform;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSceneActorSaveRecord
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FGuid ActorId;

	UPROPERTY()
	FPrimaryAssetId AssetId;

	UPROPERTY()
	FSoftClassPath ActorClass;

	UPROPERTY()
	FTransform Transform = FTransform::Identity;

	UPROPERTY()
	bool bRuntimeSpawned = false;

	UPROPERTY()
	bool bDestroyed = false;

	UPROPERTY()
	FParameter Data;
};
