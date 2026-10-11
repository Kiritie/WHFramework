#include "Ability/Abilities/ItemAbilityBase.h"

#include "Ability/Effects/EffectBase.h"
#include "Ability/Inventory/InventoryAbilitySource.h"

UItemAbilityBase::UItemAbilityBase()
{
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

bool UItemAbilityBase::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
    {
        return false;
    }
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

void UItemAbilityBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    const UInventoryAbilitySource* Source = ActorInfo ? Cast<UInventoryAbilitySource>(GetSourceObject(Handle, ActorInfo)) : nullptr;
    FAbilityItem Item;
    if (!Source || !ActorInfo->AvatarActor.IsValid() || Source->Avatar != ActorInfo->AvatarActor || !Source->ResolveItem(Item))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
    for (const TSubclassOf<UEffectBase>& EffectClass : EffectClasses)
    {
        if (!EffectClass)
        {
            continue;
        }
        const FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(EffectClass, Item.Level);
        if (Spec.IsValid())
        {
            const FActiveGameplayEffectHandle Effect = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
            if (Effect.IsValid())
            {
                EffectHandles.Add(Effect);
            }
        }
    }
}

void UItemAbilityBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (bWasCancelled)
    {
        for (const FActiveGameplayEffectHandle& Effect : EffectHandles)
        {
            BP_RemoveGameplayEffectFromOwnerWithHandle(Effect);
        }
    }
    EffectHandles.Reset();
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

TArray<TSubclassOf<UEffectBase>> UItemAbilityBase::GetEffectClasses() const
{
    TArray<TSubclassOf<UEffectBase>> Result = EffectClasses;
    Result.Append(Super::GetEffectClasses());
    return Result;
}
