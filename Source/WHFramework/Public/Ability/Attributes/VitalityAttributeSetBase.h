#pragma once

#include "AbilitySystemComponent.h"
#include "ActorAttributeSetBase.h"
#include "Ability/AbilityModuleTypes.h"

#include "VitalityAttributeSetBase.generated.h"

/**
 * 生命属性集
 */
UCLASS()
class WHFRAMEWORK_API UVitalityAttributeSetBase : public UActorAttributeSetBase
{
	GENERATED_BODY()

public:
	UVitalityAttributeSetBase();

public:
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;

	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData &Data) override;

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, SaveGame, ReplicatedUsing = OnRep_Health, Category = "VitalityAttributes")
	FGameplayAttributeData Health;
	GAMEPLAYATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, Health)
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, SaveGame, ReplicatedUsing = OnRep_MaxHealth, Category = "VitalityAttributes")
	FGameplayAttributeData MaxHealth;
	GAMEPLAYATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, MaxHealth)
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_HealthRecovery, Category = "VitalityAttributes")
	FGameplayAttributeData HealthRecovery;
	GAMEPLAYATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, HealthRecovery)

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, SaveGame, ReplicatedUsing = OnRep_HealthRegenSpeed, Category = "VitalityAttributes")
	FGameplayAttributeData HealthRegenSpeed;
	GAMEPLAYATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, HealthRegenSpeed)
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_PhysicsDamage, Category = "VitalityAttributes")
	FGameplayAttributeData PhysicsDamage;
	GAMEPLAYATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, PhysicsDamage)
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_MagicDamage, Category = "VitalityAttributes")
	FGameplayAttributeData MagicDamage;
	GAMEPLAYATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, MagicDamage)
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_FallDamage, Category = "VitalityAttributes")
	FGameplayAttributeData FallDamage;
	GAMEPLAYATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, FallDamage)
		
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_FallDamage, Category = "VitalityAttributes")
	FGameplayAttributeData RealDamage;
	GAMEPLAYATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, RealDamage)

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Interrupt, Category = "VitalityAttributes")
	FGameplayAttributeData Interrupt;
	GAMEPLAYATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, Interrupt)

protected:
	UPROPERTY(EditDefaultsOnly, Category = "VitalityHandles")
	TSubclassOf<UDamageHandle> DamageHandleClass;
	
	UPROPERTY(EditDefaultsOnly, Category = "VitalityHandles")
	TSubclassOf<URecoveryHandle> RecoveryHandleClass;
			
	UPROPERTY(EditDefaultsOnly, Category = "VitalityHandles")
	TSubclassOf<UInterruptHandle> InterruptHandleClass;

public:
	UFUNCTION()
	virtual void OnRep_Health(const FGameplayAttributeData& OldHealth);

	UFUNCTION()
	virtual void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);

	UFUNCTION()
	virtual void OnRep_HealthRecovery(const FGameplayAttributeData& OldRecovery);

	UFUNCTION()
	virtual void OnRep_HealthRegenSpeed(const FGameplayAttributeData& OldMaxHealth);

	UFUNCTION()
	virtual void OnRep_PhysicsDamage(const FGameplayAttributeData& OldPhysicsDamage);

	UFUNCTION()
	virtual void OnRep_MagicDamage(const FGameplayAttributeData& OldMagicDamage);

	UFUNCTION()
	virtual void OnRep_FallDamage(const FGameplayAttributeData& OldFallDamage);

	UFUNCTION()
	virtual void OnRep_RealDamage(const FGameplayAttributeData& OldRealDamage);

	UFUNCTION()
	virtual void OnRep_Interrupt(const FGameplayAttributeData& OldInterrupt);
};
