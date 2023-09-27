#include "Ability/AbilityModuleTypes.h"

#include "Asset/AssetModuleBPLibrary.h"
#include "Ability/Item/AbilityItemDataBase.h"
#include "Ability/Character/AbilityCharacterBase.h"
#include "Ability/Inventory/Slot/AbilityInventorySlot.h"
#include "Ability/Vitality/AbilityVitalityInterface.h"

bool FGameplayEffectContextBase::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	return Super::NetSerialize(Ar, Map, bOutSuccess) && TargetData.NetSerialize(Ar, Map, bOutSuccess);
}

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
	else if (EventData.Target.Get())
	{
		OutActors.Add(const_cast<AActor*>(EventData.Target.Get()));
	}
}

void UDamageHandle::HandleDamage(AActor* SourceActor, AActor* TargetActor, float DamageValue, EDamageType DamageType, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags)
{
	AAbilityCharacterBase* SourceCharacter = Cast<AAbilityCharacterBase>(SourceActor);
	
	IAbilityVitalityInterface* TargetVitality = Cast<IAbilityVitalityInterface>(TargetActor);

	if (DamageValue > 0.f)
	{
		if (TargetVitality && !TargetVitality->IsDead())
		{
			TargetVitality->HandleDamage(DamageType, DamageValue, false, false, HitResult, SourceTags, SourceActor);
		}
	}
}

UAbilityItemDataBase& FAbilityItemData::GetItemData() const
{
	return UAssetModuleBPLibrary::LoadPrimaryAssetRef<UAbilityItemDataBase>(AbilityID);
}

bool FGameplayEffectContainerSpec::HasValidTargets() const
{
	return TargetGameplayEffectSpecs.Num() > 0;
}

bool FGameplayEffectContainerSpec::HasValidEffects() const
{
	return TargetData.Num() > 0;
}

void FGameplayEffectContainerSpec::AddTargets(const TArray<FHitResult>& HitResults, const TArray<AActor*>& TargetActors)
{
	for (const FHitResult& HitResult : HitResults)
	{
		// 创建单个射线目标数据对象
		FGameplayAbilityTargetData_SingleTargetHit* NewData = new FGameplayAbilityTargetData_SingleTargetHit(HitResult);
		// 将目标数据加入列表
		TargetData.Add(NewData);
	}

	if (TargetActors.Num() > 0)
	{
		// 创建目标Actor数组对象
		FGameplayAbilityTargetData_ActorArray* NewData = new FGameplayAbilityTargetData_ActorArray();
		NewData->TargetActorArray.Append(TargetActors);
		// 将目标数据加入列表
		TargetData.Add(NewData);
	}
}

EAbilityItemType FAbilityItem::GetType() const
{
	return GetData().GetItemType();
}

FAbilityItems FInventorySlots::GetItems()
{
	FAbilityItems Items;
	for(const auto Iter : Slots)
	{
		Items.Items.Add(Iter->GetItem());
	}
	return Items;
}

UAbilityItemDataBase& FActorSaveData::GetItemData() const
{
	return UAssetModuleBPLibrary::LoadPrimaryAssetRef<UAbilityItemDataBase>(ID);
}
