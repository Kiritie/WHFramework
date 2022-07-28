#pragma once

#include "Ability/Attributes/AttributeSetBase.h"

#include "VitalityAttributeSetBase.generated.h"

/**
 * 生命属性集
 */
UCLASS()
class WHFRAMEWORK_API UVitalityAttributeSetBase : public UAttributeSetBase
{
	GENERATED_BODY()

public:
	UVitalityAttributeSetBase();

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, SaveGame, ReplicatedUsing = OnRep_Health, Category = "Vitality Attributes")
	FGameplayAttributeData Health;
	GAMEPLAYATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, Health)
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, SaveGame, ReplicatedUsing = OnRep_MaxHealth, Category = "Vitality Attributes")
	FGameplayAttributeData MaxHealth;
	GAMEPLAYATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, MaxHealth)

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, SaveGame, ReplicatedUsing = OnRep_Exp, Category = "Vitality Attributes")
	FGameplayAttributeData Exp;
	GAMEPLAYATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, Exp)
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, SaveGame, ReplicatedUsing = OnRep_MaxExp, Category = "Vitality Attributes")
	FGameplayAttributeData MaxExp;
	GAMEPLAYATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, MaxExp)

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_PhysicsDamage, Category = "Damage")
	FGameplayAttributeData PhysicsDamage;
	GAMEPLAYATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, PhysicsDamage)
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_MagicDamage, Category = "Damage")
	FGameplayAttributeData MagicDamage;
	GAMEPLAYATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, MagicDamage)

public:
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData &Data) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	UFUNCTION()
	virtual void OnRep_Health(const FGameplayAttributeData& OldHealth);

	UFUNCTION()
	virtual void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);

	UFUNCTION()
	virtual void OnRep_Exp(const FGameplayAttributeData& OldExp);

	UFUNCTION()
	virtual void OnRep_MaxExp(const FGameplayAttributeData& OldMaxExp);

	UFUNCTION()
	virtual void OnRep_PhysicsDamage(const FGameplayAttributeData& OldPhysicsDamage);

	UFUNCTION()
	virtual void OnRep_MagicDamage(const FGameplayAttributeData& OldMagicDamage);
};