#include "Ability/Attributes/ActorAttributeSetBase.h"

#include "GameplayEffectExtension.h"
#include "Ability/Actor/AbilityActorInterface.h"
#include "Net/UnrealNetwork.h"
#include "Ability/AbilityModuleTypes.h"
#include "ReferencePool/ReferencePoolModuleStatics.h"

UActorAttributeSetBase::UActorAttributeSetBase()
:	Exp(0.f),
	MaxExp(50.f)
{
}

void UActorAttributeSetBase::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
}

void UActorAttributeSetBase::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
}

void UActorAttributeSetBase::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);
}

void UActorAttributeSetBase::PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
}

void UActorAttributeSetBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UActorAttributeSetBase, Exp, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UActorAttributeSetBase, MaxExp, COND_None, REPNOTIFY_Always);
}

void UActorAttributeSetBase::OnRep_Exp(const FGameplayAttributeData& OldExp)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UActorAttributeSetBase, Exp, OldExp);
}

void UActorAttributeSetBase::OnRep_MaxExp(const FGameplayAttributeData& OldMaxExp)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UActorAttributeSetBase, MaxExp, OldMaxExp);
}
