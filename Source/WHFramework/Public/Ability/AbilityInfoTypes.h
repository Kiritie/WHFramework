#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "AttributeSet.h"
#include "GameplayEffect.h"
#include "SaveGame/SaveGameModuleTypes.h"

#include "AbilityInfoTypes.generated.h"

class UAbilityBase;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAbilityData : public FSaveData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FPrimaryAssetId ID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Level;

	UPROPERTY(Transient)
	FGameplayAbilitySpecHandle AbilityHandle;

public:
	FAbilityData()
	{
		ID = FPrimaryAssetId();
		Level = 1;
		AbilityHandle = FGameplayAbilitySpecHandle();
	}

	virtual bool IsValid() const override
	{
		return AbilityHandle.IsValid();
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVitalityActionAbilityData : public FAbilityData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UAbilityBase> AbilityClass;

	FORCEINLINE FVitalityActionAbilityData()
	{
		AbilityClass = nullptr;
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAttributeInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FGameplayAttribute Attribute;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FGameplayAttribute BaseAttribute;

	UPROPERTY(EditDefaultsOnly, Category=Capture)
	EGameplayEffectAttributeCaptureSource AttributeSource;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	TEnumAsByte<EGameplayModOp::Type> ModifierOp;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	float Value;

public:
	FORCEINLINE FAttributeInfo()
	{
		Attribute = FGameplayAttribute();
		BaseAttribute = FGameplayAttribute();
		AttributeSource = EGameplayEffectAttributeCaptureSource();
		ModifierOp = EGameplayModOp::Additive;
		Value = 0.f;
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEffectInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	TArray<FAttributeInfo> Attributes;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	float Duration;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	float Period;

public:
	FORCEINLINE FEffectInfo()
	{
		Attributes = TArray<FAttributeInfo>();
		Duration = 0.f;
		Period = 0.f;
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAbilityInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	TArray<FEffectInfo> Effects;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FGameplayAttribute CostAttribute;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	float CostValue;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	float CooldownDuration;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	float CooldownRemaining;

public:
	FORCEINLINE FAbilityInfo()
	{
		Effects = TArray<FEffectInfo>();
		CostAttribute = FGameplayAttribute();
		CostValue = 0.f;
		CooldownDuration = -1.f;
		CooldownRemaining = 0.f;
	}

	FORCEINLINE bool IsCooldownning() const
	{
		return CooldownRemaining > 0.f;
	}
};

USTRUCT(BlueprintType)
struct FCooldownInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly)
	float TotalTime;

	UPROPERTY(BlueprintReadOnly)
	float RemainTime;

	UPROPERTY(BlueprintReadOnly)
	bool bCooldowning;

	FCooldownInfo()
	{
		TotalTime = 0.f;
		RemainTime = 0.f;
		bCooldowning = false;
	}
};


/**
 * 查询项类型
 */
