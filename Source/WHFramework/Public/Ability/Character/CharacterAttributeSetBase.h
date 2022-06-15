#pragma once

#include "Ability/Vitality/VitalityAttributeSetBase.h"

#include "CharacterAttributeSetBase.generated.h"

/**
 * 角色属性集
 */
UCLASS()
class WHFRAMEWORK_API UCharacterAttributeSetBase : public UVitalityAttributeSetBase
{
	GENERATED_BODY()

public:
	UCharacterAttributeSetBase();

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, SaveGame, Category = "CharacterAttributes")
	FGameplayAttributeData MoveSpeed;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSetBase, MoveSpeed)

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, SaveGame, Category = "CharacterAttributes")
	FGameplayAttributeData RotationSpeed;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSetBase, RotationSpeed)

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, SaveGame, Category = "CharacterAttributes")
	FGameplayAttributeData JumpForce;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSetBase, JumpForce)

public:
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData &Data) override;
};