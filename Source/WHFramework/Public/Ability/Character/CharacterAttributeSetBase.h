#pragma once

#include "Ability/Base/AttributeSetBase.h"

#include "CharacterAttributeSetBase.generated.h"

/**
 * 角色属性集
 */
UCLASS()
class DREAMWORLD_API UCharacterAttributeSetBase : public UAttributeSetBase
{
	GENERATED_BODY()

public:
	UCharacterAttributeSetBase();

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "CharacterAttributes")
	FGameplayAttributeData MoveSpeed;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSetBase, MoveSpeed)

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "CharacterAttributes")
	FGameplayAttributeData RotationSpeed;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSetBase, RotationSpeed)

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "CharacterAttributes")
	FGameplayAttributeData JumpForce;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSetBase, JumpForce)

public:
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData &Data) override;
};