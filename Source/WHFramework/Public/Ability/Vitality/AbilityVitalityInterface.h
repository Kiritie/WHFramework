#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "SaveGame/SaveGameModuleTypes.h"
#include "Scene/SceneModuleTypes.h"

#include "AbilityVitalityInterface.generated.h"

class UAbilityBase;
UINTERFACE()
class WHFRAMEWORK_API UAbilityVitalityInterface : public UInterface
{
	GENERATED_BODY()
};

class WHFRAMEWORK_API IAbilityVitalityInterface
{
	GENERATED_BODY()

public:
	virtual void Spawn() = 0;
	
	virtual void Revive() = 0;

	virtual void Death(AActor* InKiller = nullptr) = 0;
		
	virtual void ResetData(bool bRefresh = false) = 0;

	virtual void RefreshData() = 0;
	
	virtual void ModifyHealth(float InDeltaValue) = 0;
	
	virtual void ModifyEXP(float InDeltaValue) = 0;

	virtual void AddWorldText(FString InContent, EWorldTextType InContentType, EWorldTextStyle InContentStyle) = 0;

public:
	virtual bool IsDead() const = 0;
	
public:
	virtual FPrimaryAssetId GetAssetID() const = 0;
	
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
	virtual FGameplayAbilitySpecHandle AcquireAbility(TSubclassOf<UAbilityBase> InAbility, int32 InLevel = 1) = 0;

	virtual bool ActiveAbility(FGameplayAbilitySpecHandle AbilityHandle, bool bAllowRemoteActivation = false) = 0;

	virtual bool ActiveAbilityByClass(TSubclassOf<UAbilityBase> AbilityClass, bool bAllowRemoteActivation = false) = 0;

	virtual bool ActiveAbilityByTag(const FGameplayTagContainer& GameplayTagContainer, bool bAllowRemoteActivation = false) = 0;

	virtual void CancelAbility(UAbilityBase* Ability) = 0;

	virtual void CancelAbilityByHandle(const FGameplayAbilitySpecHandle& AbilityHandle) = 0;

	virtual void CancelAbilities(const FGameplayTagContainer& WithTags, const FGameplayTagContainer& WithoutTags, UAbilityBase* Ignore = nullptr) = 0;
	
	virtual void CancelAllAbilities(UAbilityBase* Ignore = nullptr) = 0;
	
	virtual FActiveGameplayEffectHandle ApplyEffectByClass(TSubclassOf<UGameplayEffect> EffectClass) = 0;
	
	virtual FActiveGameplayEffectHandle ApplyEffectBySpecHandle(const FGameplayEffectSpecHandle& SpecHandle) = 0;
		
	virtual FActiveGameplayEffectHandle ApplyEffectBySpec(const FGameplayEffectSpec& Spec) = 0;

	virtual bool RemoveEffect(FActiveGameplayEffectHandle Handle, int32 StacksToRemove=-1) = 0;

	virtual void GetActiveAbilities(FGameplayTagContainer AbilityTags, TArray<UAbilityBase*>& ActiveAbilities) = 0;

	virtual bool GetAbilityInfo(TSubclassOf<UAbilityBase> AbilityClass, FAbilityInfo& OutAbilityInfo) = 0;

public:
	virtual void HandleDamage(EDamageType DamageType, const float LocalDamageDone, bool bHasCrited, FHitResult HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor) = 0;
		
	virtual void HandleNameChanged(FName NewValue) = 0;

	virtual void HandleRaceIDChanged(FName NewValue) = 0;
	
	virtual void HandleLevelChanged(int32 NewValue, int32 DeltaValue = 0) = 0;

	virtual void HandleEXPChanged(int32 NewValue, int32 DeltaValue = 0) = 0;

	virtual void HandleHealthChanged(float NewValue, float DeltaValue = 0.f) = 0;

	virtual void HandleMaxHealthChanged(float NewValue, float DeltaValue = 0.f) = 0;
};