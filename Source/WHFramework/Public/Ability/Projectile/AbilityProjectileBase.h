// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "Ability/Hitter/AbilityHitterInterface.h"
#include "Common/Base/WHActor.h"
#include "AbilityProjectileBase.generated.h"

/**
 * 技能基类
 */
UCLASS()
class WHFRAMEWORK_API AAbilityProjectileBase : public AWHActor, public IAbilityHitterInterface
{
	GENERATED_BODY()
	
public:	
	AAbilityProjectileBase();
	
	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:
	virtual int32 GetLimit_Implementation() const override { return -1; }

	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;
		
	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	virtual void OnPreparatory_Implementation() override;
	
	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

protected:
	virtual bool IsUseDefaultLifecycle_Implementation() const override {  return true;}
	
public:
	UFUNCTION(BlueprintNativeEvent)
	void Launch(FVector InDirection = FVector::ZeroVector);
	
	UFUNCTION(BlueprintNativeEvent)
	void Destroy();

public:
	virtual bool CanHitTarget(AActor* InTarget) const override;

	virtual void OnHitTarget(AActor* InTarget, const FHitResult& InHitResult) override;

	virtual bool IsHitAble() const override;

	virtual void SetHitAble(bool bValue) override;

	virtual void ClearHitTargets() override;

	virtual TArray<AActor*> GetHitTargets() const override;

protected:
	FGameplayEffectContainerSpec MakeEffectContainerSpecFromContainer(const FGameplayEffectContainer& InContainer, const FGameplayEventData& EventData, int32 OverrideGameplayLevel);

	FGameplayEffectContainerSpec MakeEffectContainerSpec(FGameplayTag ContainerTag, const FGameplayEventData& EventData, int32 OverrideGameplayLevel);

	TArray<FActiveGameplayEffectHandle> ApplyEffectContainerSpec(const FGameplayEffectContainerSpec& ContainerSpec);

	TArray<FActiveGameplayEffectHandle> ApplyEffectContainer(FGameplayTag ContainerTag, const FGameplayEventData& EventData, int32 OverrideGameplayLevel);

	TArray<FActiveGameplayEffectHandle> ApplyGameplayEffectSpecToTarget(const FGameplayEffectSpecHandle SpecHandle, const FGameplayAbilityTargetDataHandle& TargetData) const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName OriginSocketName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName FinalSocketName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float MaxDurationTime;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag HitEventTag;

	UPROPERTY()
	AActor* OwnerActor;

	UPROPERTY()
	int32 AbilityLevel;

	UPROPERTY()
	FGameplayAbilityActorInfo AbilityActorInfo;

	UPROPERTY()
	TMap<FGameplayTag, FGameplayEffectContainer> EffectContainerMap;

	UPROPERTY()
	TArray<AActor*> HitTargets;

	UPROPERTY()
	bool bLaunched;

private:
	FTimerHandle DestroyTimer;

public:
	template<class T>
	T* GetOwnerActor() const
	{
		return Cast<T>(OwnerActor);
	}

	UFUNCTION(BlueprintPure)
	AActor* GetOwnerActor() const { return OwnerActor; }

	UFUNCTION(BlueprintPure)
	bool IsLaunched() const { return bLaunched; }
};
