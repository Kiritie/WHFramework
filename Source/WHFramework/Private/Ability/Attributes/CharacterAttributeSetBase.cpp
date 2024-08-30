#include "Ability/Attributes/CharacterAttributeSetBase.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffectExtension.h"
#include "Ability/Character/AbilityCharacterBase.h"
#include "Net/UnrealNetwork.h"

UCharacterAttributeSetBase::UCharacterAttributeSetBase()
:	
	MoveSpeed(350.f),
	RotationSpeed(360.f),
	JumpForce(420.f)
{
}

void UCharacterAttributeSetBase::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	UAbilitySystemComponent* AbilityComp = GetOwningAbilitySystemComponent();
	AAbilityCharacterBase* TargetCharacter = Cast<AAbilityCharacterBase>(AbilityComp->GetAvatarActor());
	
	const float CurrentValue = Attribute.GetGameplayAttributeData(this)->GetCurrentValue();
	if (Attribute == GetMoveSpeedAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, NewValue);
	}
	else if (Attribute == GetRotationSpeedAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, NewValue);
	}
	else if (Attribute == GetJumpForceAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, NewValue);
	}
}

void UCharacterAttributeSetBase::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	UAbilitySystemComponent* AbilityComp = GetOwningAbilitySystemComponent();
	AAbilityCharacterBase* TargetCharacter = Cast<AAbilityCharacterBase>(AbilityComp->GetAvatarActor());
	
	const float CurrentValue = Attribute.GetGameplayAttributeData(this)->GetCurrentValue();
	if (Attribute == GetMoveSpeedAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, NewValue);
	}
	else if (Attribute == GetRotationSpeedAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, NewValue);
	}
	else if (Attribute == GetJumpForceAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, NewValue);
	}
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

void UCharacterAttributeSetBase::OnRep_JumpForce(const FGameplayAttributeData& OldJumpForce)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCharacterAttributeSetBase, JumpForce, OldJumpForce);
}
