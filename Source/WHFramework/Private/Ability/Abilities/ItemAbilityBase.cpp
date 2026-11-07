#include "Ability/Abilities/ItemAbilityBase.h"
#include "Ability/Inventory/InventoryItemTypes.h"

#include "AbilitySystemComponent.h"
#include "Ability/Inventory/InventoryAbilitySource.h"

UItemAbilityBase::UItemAbilityBase()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	ActionTag = AbilityActionTags::ItemUse;
	bActivateOnInput = false;
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
