#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AbilityEffectSaveTypes.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAbilityEffectSaveData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FSoftClassPath EffectClass;

	UPROPERTY(SaveGame)
	FGuid SourceId;

	UPROPERTY(SaveGame)
	FGuid EffectInstanceId;

	UPROPERTY(SaveGame)
	FSoftObjectPath SourceObject;

	UPROPERTY(SaveGame)
	FGameplayTagContainer CapturedSourceTags;

	UPROPERTY(SaveGame)
	FGameplayTagContainer DynamicGrantedTags;

	UPROPERTY(SaveGame)
	FGameplayTagContainer DynamicAssetTags;

	UPROPERTY(SaveGame)
	TMap<FName, float> SetByCallerNameMagnitudes;

	UPROPERTY(SaveGame)
	float Level = 1.f;

	UPROPERTY(SaveGame)
	int32 StackCount = 1;

	UPROPERTY(SaveGame)
	float RemainingDuration = -1.f;

	UPROPERTY(SaveGame)
	double SavedUtcSeconds = 0.0;

	UPROPERTY(SaveGame)
	TMap<FGameplayTag, float> SetByCallerMagnitudes;
};
