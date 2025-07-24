#include "Ability/Attributes/CharacterAttributeSetBase.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UCharacterAttributeSetBase::UCharacterAttributeSetBase()
	: MoveSpeed(350.f)
	, RotationSpeed(720.f)
	, SwimSpeed(350.f)
	, FlySpeed(350.f)
	, JumpForce(420.f)
{
}

void UCharacterAttributeSetBase::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	
	if (Attribute == GetMoveSpeedAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, NewValue);
	}
	else if (Attribute == GetRotationSpeedAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, NewValue);
	}
	else if (Attribute == GetSwimSpeedAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, NewValue);
	}
	else if (Attribute == GetFlySpeedAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, NewValue);
	}
	else if (Attribute == GetJumpForceAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, NewValue);
	}
}

void UCharacterAttributeSetBase::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);
}

void UCharacterAttributeSetBase::PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
}

void UCharacterAttributeSetBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UCharacterAttributeSetBase, MoveSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCharacterAttributeSetBase, RotationSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCharacterAttributeSetBase, SwimSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCharacterAttributeSetBase, FlySpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCharacterAttributeSetBase, JumpForce, COND_None, REPNOTIFY_Always);
}

void UCharacterAttributeSetBase::OnRep_MoveSpeed(const FGameplayAttributeData& OldMoveSpeed)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCharacterAttributeSetBase, MoveSpeed, OldMoveSpeed);
}

void UCharacterAttributeSetBase::OnRep_RotationSpeed(const FGameplayAttributeData& OldRotationSpeed)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCharacterAttributeSetBase, RotationSpeed, OldRotationSpeed);
}

void UCharacterAttributeSetBase::OnRep_SwimSpeed(const FGameplayAttributeData& OldSwimSpeed)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCharacterAttributeSetBase, SwimSpeed, OldSwimSpeed);
}

void UCharacterAttributeSetBase::OnRep_FlySpeed(const FGameplayAttributeData& OldFlySpeed)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCharacterAttributeSetBase, FlySpeed, OldFlySpeed);
}

void UCharacterAttributeSetBase::OnRep_JumpForce(const FGameplayAttributeData& OldJumpForce)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCharacterAttributeSetBase, JumpForce, OldJumpForce);
}
