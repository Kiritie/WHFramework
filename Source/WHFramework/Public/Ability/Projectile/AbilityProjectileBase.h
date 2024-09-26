// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "Ability/Interfaces/AbilityHitterInterface.h"
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

protected:
	virtual int32 GetLimit_Implementation() const override { return 1000; }

	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation(bool bRecovery) override;
	
public:
	UFUNCTION(BlueprintNativeEvent)
	void Initialize(AActor* InOwnerActor, const FGameplayAbilitySpecHandle& InAbilityHandle);
	
public:
	virtual bool CanHitTarget(AActor* InTarget) const override;

	virtual void OnHitTarget(AActor* InTarget, const FHitResult& InHitResult) override;
	
	virtual void ClearHitTargets() override;

	virtual void SetHitAble(bool bValue) override;

protected:
	FGameplayEffectContainerSpec MakeEffectContainerSpecFromContainer(const FGameplayEffectContainer& InContainer, const FGameplayEventData& EventData, int32 OverrideGameplayLevel);

	FGameplayEffectContainerSpec MakeEffectContainerSpec(FGameplayTag ContainerTag, const FGameplayEventData& EventData, int32 OverrideGameplayLevel);

	TArray<FActiveGameplayEffectHandle> ApplyEffectContainerSpec(const FGameplayEffectContainerSpec& ContainerSpec);

	TArray<FActiveGameplayEffectHandle> ApplyEffectContainer(FGameplayTag ContainerTag, const FGameplayEventData& EventData, int32 OverrideGameplayLevel);

	TArray<FActiveGameplayEffectHandle> ApplyGameplayEffectSpecToTarget(const FGameplayEffectSpecHandle SpecHandle, const FGameplayAbilityTargetDataHandle& TargetData) const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName SocketName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float DurationTime;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag HitEventTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TMap<FGameplayTag, FGameplayEffectContainer> EffectContainerMap;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 Level;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	AActor* OwnerActor;

	UPROPERTY()
	TArray<AActor*> HitTargets;

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
};
