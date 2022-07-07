#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "Ability/Actor/AbilityActorInterface.h"
#include "SaveGame/SaveGameModuleTypes.h"
#include "Scene/SceneModuleTypes.h"

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
	
	virtual void Revive(IAbilityVitalityInterface* InRescuer) = 0;
	
	virtual void ModifyHealth(float InDeltaValue) = 0;
	
	virtual void ModifyEXP(float InDeltaValue) = 0;

public:
	virtual bool IsDead(bool bCheckDying = false) const = 0;
	
	virtual bool IsDying() const = 0;
	
public:
	virtual FName GetNameV() const = 0;

	virtual void SetNameV(FName InName) = 0;
		
	virtual FName GetRaceID() const = 0;

	virtual void SetRaceID(FName InRaceID) = 0;
	
	virtual int32 GetLevelV() const = 0;

	virtual void SetLevelV(int32 InLevel) = 0;

	virtual int32 GetEXP() const = 0;
				
	virtual void SetEXP(int32 InEXP) = 0;

	virtual int32 GetBaseEXP() const = 0;
	
	virtual int32 GetEXPFactor() const = 0;

	virtual int32 GetMaxEXP() const = 0;

	virtual int32 GetTotalEXP() const = 0;

	virtual FString GetHeadInfo() const = 0;
		
	virtual float GetHealth() const = 0;
				
	virtual void SetHealth(float InValue)  = 0;

	virtual float GetMaxHealth() const = 0;
			
	virtual void SetMaxHealth(float InValue) = 0;

	virtual float GetPhysicsDamage() const = 0;
		
	virtual float GetMagicDamage() const = 0;

public:
	virtual void HandleDamage(EDamageType DamageType, const float LocalDamageDone, bool bHasCrited, FHitResult HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor) = 0;
};
