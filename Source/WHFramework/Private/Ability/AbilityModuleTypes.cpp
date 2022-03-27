#include "Ability/AbilityModuleTypes.h"

#include "Asset/Primary/Item/ItemAssetBase.h"

FItem FItem::Empty = FItem();

void UTargetType::GetTargets_Implementation(AActor* OwningActor, AActor* TargetingActor, FGameplayEventData EventData, TArray<FHitResult>& OutHitResults, TArray<AActor*>& OutActors) const
{
	
}

void UTargetType_UseOwner::GetTargets_Implementation(AActor* OwningActor, AActor* TargetingActor, FGameplayEventData EventData, TArray<FHitResult>& OutHitResults, TArray<AActor*>& OutActors) const
{
	OutActors.Add(OwningActor);
}

void UTargetType_UseEventData::GetTargets_Implementation(AActor* OwningActor, AActor* TargetingActor, FGameplayEventData EventData, TArray<FHitResult>& OutHitResults, TArray<AActor*>& OutActors) const
{
	const FHitResult* FoundHitResult = EventData.ContextHandle.GetHitResult();
	if (FoundHitResult)
	{
		OutHitResults.Add(*FoundHitResult);
	}
	else if (EventData.Target)
	{
		OutActors.Add(const_cast<AActor*>(EventData.Target));
	}
}

UItemAssetBase& FAbilityData::GetItemData() const
{
	return UPrimaryAssetManager::LoadItemAsset(AbilityID);
}
