#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "AbilityGrantTypes.generated.h"

class UAbilityBase;
class UEffectBase;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAbilityGrantDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FPrimaryAssetId DefinitionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UAbilityBase> AbilityClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UObject> SourceObject = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Level = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bActivateOnGrant = false;

	/** Consumable specs expose admission/cooldown configuration; their inventory transaction owns execution. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bActionEnabled = true;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAbilityGrantSourceDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGuid SourceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FAbilityGrantDefinition> Abilities;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<TSubclassOf<UEffectBase>> Effects;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Level = 1;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAbilityGrantedEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FGuid SourceId;

	UPROPERTY(BlueprintReadOnly)
	FPrimaryAssetId DefinitionId;

	UPROPERTY(BlueprintReadOnly)
	FGameplayTag ActionTag;

	UPROPERTY(BlueprintReadOnly)
	FGameplayAbilitySpecHandle AbilityHandle;

	UPROPERTY(BlueprintReadOnly)
	int32 Level = 1;

	UPROPERTY(BlueprintReadOnly)
	bool bActionEnabled = true;
};

USTRUCT()
struct WHFRAMEWORK_API FAbilityGrantSourceState
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FAbilityGrantDefinition> Abilities;

	UPROPERTY()
	TArray<TSubclassOf<UEffectBase>> Effects;

	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> AbilityHandles;

	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> EffectHandles;

	UPROPERTY()
	int32 Level = 1;
};
