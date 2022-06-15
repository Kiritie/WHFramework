#include "Ability/Character/CharacterAttributeSetBase.h"

#include "GameplayEffectExtension.h"
#include "Ability/Character/AbilityCharacterBase.h"

UCharacterAttributeSetBase::UCharacterAttributeSetBase()
:	
	MoveSpeed(350.f),
	RotationSpeed(360.f),
	JumpForce(420.f)
{
}

void UCharacterAttributeSetBase::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	UAbilitySystemComponent* AbilityComp = GetOwningAbilitySystemComponent();
	AAbilityCharacterBase* TargetCharacter = Cast<AAbilityCharacterBase>(AbilityComp->GetAvatarActor());
	
	const float CurrentValue = Attribute.GetGameplayAttributeData(this)->GetCurrentValue();
	if (Attribute == GetMoveSpeedAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, NewValue);
		if (TargetCharacter)
		{
			TargetCharacter->HandleMoveSpeedChanged(NewValue, NewValue - CurrentValue);
		}
	}
	else if (Attribute == GetRotationSpeedAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, NewValue);
		if (TargetCharacter)
		{
			TargetCharacter->HandleRotationSpeedChanged(NewValue, NewValue - CurrentValue);
		}
	}
	else if (Attribute == GetJumpForceAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, NewValue);
		if (TargetCharacter)
		{
			TargetCharacter->HandleJumpForceChanged(NewValue, NewValue - CurrentValue);
		}
	}
	else
	{
		Super::PreAttributeChange(Attribute, NewValue);
	}
}

void UCharacterAttributeSetBase::PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
}
