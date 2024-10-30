#pragma once

#include "Ability/Actor/AbilityActorInterface.h"

#include "AbilityVitalityInterface.generated.h"

class UAbilityBase;
UINTERFACE()
class WHFRAMEWORK_API UAbilityVitalityInterface : public UAbilityActorInterface
{
	GENERATED_BODY()
};

class WHFRAMEWORK_API IAbilityVitalityInterface : public IAbilityActorInterface
{
	GENERATED_BODY()

public:
	virtual void Death(IAbilityVitalityInterface* InKiller) = 0;
	
	virtual void Kill(IAbilityVitalityInterface* InTarget) = 0;
	
	virtual void Revive(IAbilityVitalityInterface* InRescuer) = 0;

	virtual void Static() = 0;

	virtual void UnStatic() = 0;

	virtual void Interrupt(float InDuration = -1.f) = 0;

	virtual void UnInterrupt() = 0;

	virtual void ModifyHealth(float InDeltaValue) = 0;

public:
	virtual bool IsActive(bool bNeedNotDead = false) const = 0;

	virtual bool IsDead(bool bCheckDying = true) const = 0;
	
	virtual bool IsDying() const = 0;
	
	virtual bool IsWalking(bool bReally = false) const = 0;
	
	virtual bool IsInterrupting() const = 0;

public:
	virtual FName GetRaceID() const = 0;

	virtual void SetRaceID(FName InRaceID) = 0;

	virtual FString GetHeadInfo() const = 0;

public:
	virtual void HandleDamage(EDamageType DamageType, const float LocalDamageDone, bool bHasCrited, bool bHasDefend, FHitResult HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor) = 0;
	
	virtual void HandleRecovery(const float LocalRecoveryDone, FHitResult HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor) = 0;
	
	virtual void HandleInterrupt(const float InterruptDuration, FHitResult HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor) = 0;
};
