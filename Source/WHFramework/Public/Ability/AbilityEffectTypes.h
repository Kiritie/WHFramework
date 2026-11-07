#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"
#include "UObject/StrongObjectPtr.h"
#include "Ability/AbilityTargetTypes.h"

#include "AbilityEffectTypes.generated.h"

class UEffectBase;

USTRUCT()
struct WHFRAMEWORK_API FGameplayEffectContextBase : public FGameplayEffectContext
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY()
	FGuid SourceId;

	UPROPERTY()
	FGuid EffectInstanceId;

	UPROPERTY()
	FGuid GrantSourceId;

	/** A committed external domain owns this effect's save/restore record. */
	UPROPERTY()
	FName ExternalPersistenceDomain;

	TStrongObjectPtr<UObject> SourceDefinition;

	virtual void AddSourceObject(const UObject* NewSourceObject) override
	{
		Super::AddSourceObject(NewSourceObject);
		SourceDefinition.Reset(NewSourceObject && (NewSourceObject->IsAsset() || NewSourceObject->HasAnyFlags(RF_ClassDefaultObject)) ? const_cast<UObject*>(NewSourceObject) : nullptr);
	}


	virtual FGameplayAbilityTargetDataHandle GetTargetData()
	{
		return TargetData;
	}

	virtual void AddTargetData(const FGameplayAbilityTargetDataHandle& TargetDataHandle)
	{
		TargetData.Append(TargetDataHandle);
	}

	/**
	* Functions that subclasses of FGameplayEffectContext need to override
	*/

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FGameplayEffectContextBase::StaticStruct();
	}

	virtual FGameplayEffectContextBase* Duplicate() const override
	{
		FGameplayEffectContextBase* NewContext = new FGameplayEffectContextBase();
		*NewContext = *this;
		if (GetHitResult())
		{
			// Does a deep copy of the hit result
			NewContext->AddHitResult(*GetHitResult(), true);
		}
		return NewContext;
	}

	virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess) override;

protected:
	FGameplayAbilityTargetDataHandle TargetData;
};

template<>
struct TStructOpsTypeTraits<FGameplayEffectContextBase> : public TStructOpsTypeTraitsBase2<FGameplayEffectContextBase>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true		// Necessary so that TSharedPtr<FHitResult> Data is copied around
	};
};

USTRUCT(BlueprintType)
struct FGameplayEffectContainer
{
	GENERATED_BODY()

public:
    FGameplayEffectContainer() {}

public:
    /** 目标类型 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GameplayEffectContainer")
    TSubclassOf<UTargetType> TargetType;

    /** 目标GameplayEffect类型 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GameplayEffectContainer")
    TArray<TSubclassOf<UEffectBase>> TargetGameplayEffectClasses;
};

/**
 * GameplayEffect容器规格
 */
USTRUCT(BlueprintType)
struct FGameplayEffectContainerSpec
{
    GENERATED_BODY()

public:
    FGameplayEffectContainerSpec() {}

public:
    /** 目标数据 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GameplayEffectContainer")
    FGameplayAbilityTargetDataHandle TargetData;

    /** 目标GameplayEffect规格 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GameplayEffectContainer")
    TArray<FGameplayEffectSpecHandle> TargetGameplayEffectSpecs;

    /** 是否有有效目标 */
    bool HasValidTargets() const;

    /** 是否有有效Effect */
    bool HasValidEffects() const;

    /** 添加目标 */
    void AddTargets(const TArray<FHitResult>& HitResults, const TArray<AActor*>& TargetActors);
};
