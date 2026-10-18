#include "Ability/Actor/AbilityActorInterface.h"

#include "Ability/Attributes/AttributeSetBase.h"
#include "Ability/Components/AbilitySystemComponentBase.h"
#include "Components/MeshComponent.h"

void IAbilityActorInterface::InitializeAbilities(AActor* InOwnerActor, AActor* InAvatarActor)
{
	UAbilitySystemComponentBase* AbilitySystem = Cast<UAbilitySystemComponentBase>(GetAbilitySystemComponent());
	UAttributeSetBase* Attributes = GetAttributeSet();
	AActor* InterfaceActor = Cast<AActor>(_getUObject());
	AActor* Owner = InOwnerActor ? InOwnerActor : (AbilitySystem ? AbilitySystem->GetOwner() : InterfaceActor);
	AActor* Avatar = InAvatarActor ? InAvatarActor : InterfaceActor;
	if (!AbilitySystem || !Attributes || !Owner || !Avatar)
	{
		return;
	}
	if (bAbilitiesInitialized && BoundAbilitySystem == AbilitySystem && BoundAbilityAvatar == Avatar)
	{
		AbilitySystem->BindOwnerAvatar(Owner, Avatar);
		return;
	}
	UninitializeAbilities(false);
	if (!AbilitySystem->BindOwnerAvatar(Owner, Avatar))
	{
		return;
	}
	BoundAbilitySystem = AbilitySystem;
	BoundAbilityAvatar = Avatar;
	for (const FGameplayAttribute& Attribute : Attributes->GetAllAttributes())
	{
		const FDelegateHandle Delegate = AbilitySystem->GetGameplayAttributeValueChangeDelegate(Attribute).AddWeakLambda(_getUObject(), [this](const FOnAttributeChangeData& Change)
		{
			OnAttributeChange(Change);
		});
		AttributeDelegateHandles.Add(Attribute, Delegate);
	}
	bAbilitiesInitialized = true;
	OnAbilityRuntimeChanged.Broadcast();
}

void IAbilityActorInterface::UninitializeAbilities(bool bUnbindAvatar)
{
	const bool bWasInitialized = bAbilitiesInitialized;
	if (UAbilitySystemComponentBase* AbilitySystem = BoundAbilitySystem.Get())
	{
		for (const TPair<FGameplayAttribute, FDelegateHandle>& Entry : AttributeDelegateHandles)
		{
			AbilitySystem->GetGameplayAttributeValueChangeDelegate(Entry.Key).Remove(Entry.Value);
		}
		if (bUnbindAvatar && AbilitySystem->GetAvatarActor() == BoundAbilityAvatar.Get())
		{
			AbilitySystem->UnbindAvatar(AbilityActionTags::AvatarRemoved);
		}
	}
	AttributeDelegateHandles.Reset();
	BoundAbilitySystem.Reset();
	BoundAbilityAvatar.Reset();
	bAbilitiesInitialized = false;
	if (bWasInitialized)
	{
		OnAbilityRuntimeChanged.Broadcast();
	}
}

void IAbilityActorInterface::RefreshAttributes()
{
	UAttributeSetBase* Attributes = GetAttributeSet();
	if (!Attributes)
	{
		return;
	}
	for (const FGameplayAttribute& Attribute : Attributes->GetAllAttributes())
	{
		FOnAttributeChangeData Change;
		Change.Attribute = Attribute;
		Change.OldValue = Attributes->GetAttributeValue(Attribute);
		Change.NewValue = Change.OldValue;
		OnAttributeChange(Change);
	}
}

bool IAbilityActorInterface::AttachActor(AActor* InActor, const FAttachmentTransformRules& InAttachmentRules, FName InSocketName)
{
	if (IsValid(InActor) && GetMeshComponent() && InActor->AttachToComponent(GetMeshComponent(), InAttachmentRules, InSocketName))
	{
		OnActorAttached(InActor);
		return true;
	}
	return false;
}

void IAbilityActorInterface::DetachActor(AActor* InActor, const FDetachmentTransformRules& InDetachmentRules)
{
	if (IsValid(InActor))
	{
		InActor->DetachFromActor(InDetachmentRules);
		OnActorDetached(InActor);
	}
}
