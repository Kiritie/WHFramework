#include "Ability/Abilities/ItemAbilityBase.h"

#include "Ability/Effects/EffectBase.h"
#include "AbilitySystemComponent.h"
#include "Ability/Inventory/InventoryAbilitySource.h"

UItemAbilityBase::UItemAbilityBase()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	ActionTag = AbilityActionTags::ItemUse;
	bActivateOnInput = false;
}

void UItemAbilityBase::BeginAction_Implementation()
{
	EffectHandles.Reset();
	const FGuid ApplyingId = GetActionActivationId();
	TWeakObjectPtr<UAbilitySystemComponent> ApplyingSystem = GetAbilitySystemComponentFromActorInfo();
	for (TSubclassOf<UEffectBase> EffectClass : EffectClasses)
	{
		const FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(EffectClass, GetAbilityLevel());
		if (!Spec.IsValid())
		{
			FailAction(AbilityActionTags::InvalidRequest);
			return;
		}
		const FActiveGameplayEffectHandle Applied = ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, Spec);
		if (!IsActive() || GetActionActivationId() != ApplyingId)
		{
			if (ApplyingSystem.IsValid() && Applied.IsValid())
			{
				ApplyingSystem->RemoveActiveGameplayEffect(Applied);
			}
			return;
		}
		if (!Applied.WasSuccessfullyApplied())
		{
			FailAction(AbilityActionTags::ActivationFailed);
			return;
		}
		if (Applied.IsValid())
		{
			EffectHandles.Add(Applied);
		}
	}
	CompleteAction();
}

void UItemAbilityBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (!IsActive() || bCleaningEffects)
	{
		return;
	}
	bCleaningEffects = true;
	const FGuid EndingId = GetActionActivationId();
	const TArray<FActiveGameplayEffectHandle> Applied = MoveTemp(EffectHandles);
	if (bWasCancelled)
	{
		for (FActiveGameplayEffectHandle Effect : Applied)
		{
			BP_RemoveGameplayEffectFromOwnerWithHandle(Effect);
		}
	}
	bCleaningEffects = false;
	if (IsActive() && GetActionActivationId() == EndingId)
	{
		Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	}
}

TArray<TSubclassOf<UEffectBase>> UItemAbilityBase::GetEffectClasses() const
{
	TArray<TSubclassOf<UEffectBase>> Result = EffectClasses;
	Result.Append(Super::GetEffectClasses());
	return Result;
}

bool UItemAbilityBase::CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid() || !ActorInfo->IsNetAuthority()) return false;
	const UInventoryAbilitySource* Source = Cast<UInventoryAbilitySource>(GetSourceObject(Handle, ActorInfo));
	FAbilityItem Item;
	return Source && Source->Avatar == ActorInfo->AvatarActor && Source->ResolveItem(Item)
		&& Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

bool UItemAbilityBase::GetInventoryItem(FAbilityItem& OutItem) const
{
	OutItem = FAbilityItem();
	const FGameplayAbilitySpec* Spec = GetCurrentAbilitySpec();
	const UInventoryAbilitySource* Source = Spec ? Cast<UInventoryAbilitySource>(Spec->SourceObject.Get()) : nullptr;
	return Source && Source->ResolveItem(OutItem);
}
