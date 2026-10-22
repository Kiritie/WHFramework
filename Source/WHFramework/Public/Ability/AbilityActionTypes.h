#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayAbilitySpec.h"
#include "NativeGameplayTags.h"
#include "AbilityActionTypes.generated.h"

namespace AbilityActionTags
{
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Request);
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(ItemUse);
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(StaticControl);
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InvalidRequest);
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(NotReady);
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(NotGranted);
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(AlreadyActive);
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(ActivationFailed);
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InvalidTarget);
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(OutOfRange);
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Blocked);
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cancelled);
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(AvatarRemoved);
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SourceRemoved);
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TargetDataTimeout);
}

UENUM(BlueprintType)
enum class EAbilityActionOutcome : uint8
{
	None,
	Completed,
	Cancelled,
	Rejected,
	Failed
};

UENUM(BlueprintType)
enum class EAbilityActionTargetPolicy : uint8
{
	None,
	Actor,
	Location
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAbilityActionRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayAbilitySpecHandle AbilityHandle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FPrimaryAssetId DefinitionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag ActionTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector AimDirection = FVector::ForwardVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector MovementDirection = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag Variant;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 WeaponPart = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ComboIndex = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAbilityActionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FGuid ActivationId;

	UPROPERTY(BlueprintReadOnly)
	FGameplayAbilitySpecHandle AbilityHandle;

	UPROPERTY(BlueprintReadOnly)
	EAbilityActionOutcome Outcome = EAbilityActionOutcome::None;

	UPROPERTY(BlueprintReadOnly)
	FGameplayTag Reason;

	UPROPERTY(BlueprintReadOnly)
	FGameplayTagContainer FailureTags;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAbilityActionStartResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FGuid ActivationId;

	UPROPERTY(BlueprintReadOnly)
	bool bAccepted = false;
};

USTRUCT()
struct WHFRAMEWORK_API FAbilityActionTargetData : public FGameplayAbilityTargetData
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid ActivationId;
	UPROPERTY()
	FAbilityActionRequest Request;

	virtual UScriptStruct* GetScriptStruct() const override;
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
	static bool Read(const FGameplayEventData* EventData, FGuid& OutActivationId, FAbilityActionRequest& OutRequest);
};

template<>
struct TStructOpsTypeTraits<FAbilityActionTargetData> : public TStructOpsTypeTraitsBase2<FAbilityActionTargetData>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnAbilityActionFinished, const FAbilityActionResult&);
