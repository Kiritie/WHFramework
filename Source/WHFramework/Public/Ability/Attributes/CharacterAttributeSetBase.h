#pragma once

#include "AbilitySystemComponent.h"
#include "Ability/AbilityModuleTypes.h"
#include "Ability/Attributes/VitalityAttributeSetBase.h"

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
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;

	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData &Data) override;

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, SaveGame, ReplicatedUsing = OnRep_MoveSpeed, Category = "CharacterAttributes")
	FGameplayAttributeData MoveSpeed;
	GAMEPLAYATTRIBUTE_ACCESSORS(UCharacterAttributeSetBase, MoveSpeed)

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, SaveGame, ReplicatedUsing = OnRep_RotationSpeed, Category = "CharacterAttributes")
	FGameplayAttributeData RotationSpeed;
	GAMEPLAYATTRIBUTE_ACCESSORS(UCharacterAttributeSetBase, RotationSpeed)
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "CharacterAttributes")
	FGameplayAttributeData SwimSpeed;
	GAMEPLAYATTRIBUTE_ACCESSORS(UCharacterAttributeSetBase, SwimSpeed)
		
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "CharacterAttributes")
	FGameplayAttributeData FlySpeed;
	GAMEPLAYATTRIBUTE_ACCESSORS(UCharacterAttributeSetBase, FlySpeed)

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, SaveGame, ReplicatedUsing = OnRep_JumpForce, Category = "CharacterAttributes")
	FGameplayAttributeData JumpForce;
	GAMEPLAYATTRIBUTE_ACCESSORS(UCharacterAttributeSetBase, JumpForce)

public:
	UFUNCTION()
	virtual void OnRep_MoveSpeed(const FGameplayAttributeData& OldMoveSpeed);

	UFUNCTION()
	virtual void OnRep_RotationSpeed(const FGameplayAttributeData& OldRotationSpeed);

	UFUNCTION()
	virtual void OnRep_SwimSpeed(const FGameplayAttributeData& OldSwimSpeed);

	UFUNCTION()
	virtual void OnRep_FlySpeed(const FGameplayAttributeData& OldFlySpeed);

	UFUNCTION()
	virtual void OnRep_JumpForce(const FGameplayAttributeData& OldJumpForce);
};
