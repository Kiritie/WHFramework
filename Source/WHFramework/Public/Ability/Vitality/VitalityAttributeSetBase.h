#pragma once

#include "Ability/Base/AttributeSetBase.h"

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
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, SaveGame, Category = "Vitality Attributes")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, Health)
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, SaveGame, Category = "Vitality Attributes")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, MaxHealth)

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Damage")
	FGameplayAttributeData PhysicsDamage;
	ATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, PhysicsDamage)
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Damage")
	FGameplayAttributeData MagicDamage;
	ATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, MagicDamage)

public:
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData &Data) override;
};