#include "Ability/AbilityModuleTypes.h"

#include "Asset/AssetModuleStatics.h"
#include "Ability/Item/AbilityItemDataBase.h"
#include "Ability/Character/AbilityCharacterBase.h"
#include "Ability/Inventory/AbilityInventoryBase.h"
#include "Ability/Inventory/Slot/AbilityInventorySlotBase.h"
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
	IAbilityVitalityInterface* TargetVitality = Cast<IAbilityVitalityInterface>(TargetActor);

	if (DamageValue > 0.f)
	{
		if (TargetVitality && !TargetVitality->IsDead())
		{
			TargetVitality->HandleDamage(DamageType, DamageValue, false, false, HitResult, SourceTags, SourceActor);
		}
	}
}

void URecoveryHandle::HandleRecovery(AActor* SourceActor, AActor* TargetActor, float RecoveryValue, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags)
{
	IAbilityVitalityInterface* TargetVitality = Cast<IAbilityVitalityInterface>(TargetActor);

	if (RecoveryValue > 0.f)
	{
		if (TargetVitality && !TargetVitality->IsDead())
		{
			TargetVitality->HandleRecovery(RecoveryValue, HitResult, SourceTags, SourceActor);
		}
	}
}

void UInterruptHandle::HandleInterrupt(AActor* SourceActor, AActor* TargetActor, float InterruptDuration, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags)
{
	IAbilityVitalityInterface* TargetVitality = Cast<IAbilityVitalityInterface>(TargetActor);

	if (InterruptDuration > 0.f)
	{
		if (TargetVitality && !TargetVitality->IsDead())
		{
			TargetVitality->HandleInterrupt(InterruptDuration, HitResult, SourceTags, SourceActor);
		}
	}
}

UAbilityItemDataBase& FAbilityItemData::GetItemData() const
{
	return UAssetModuleStatics::LoadPrimaryAssetRef<UAbilityItemDataBase>(AbilityID);
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

void FInventorySaveData::CopyAllItem(FInventorySaveData SaveData)
{
	SplitItems = SaveData.SplitItems;
}

void FInventorySaveData::AddItem(FAbilityItem InItem)
{
	UAbilityInventoryBase& Inventory = UReferencePoolModuleStatics::GetReference<UAbilityInventoryBase>(false, InventoryClass);

	Inventory.LoadSaveData(this);
	Inventory.AddItemByRange(InItem, 0, -1, false);

	CopyAllItem(Inventory.GetSaveDataRef<FInventorySaveData>(true));
}

void FInventorySaveData::RemoveItem(FAbilityItem InItem)
{
	UAbilityInventoryBase& Inventory = UReferencePoolModuleStatics::GetReference<UAbilityInventoryBase>(false, InventoryClass);

	Inventory.LoadSaveData(this);
	Inventory.RemoveItemByRange(InItem, 0, -1);

	CopyAllItem(Inventory.GetSaveDataRef<FInventorySaveData>(true));
}

void FInventorySaveData::ClearItem(FAbilityItem InItem)
{
	UAbilityInventoryBase& Inventory = UReferencePoolModuleStatics::GetReference<UAbilityInventoryBase>(false, InventoryClass);

	Inventory.LoadSaveData(this);
	Inventory.ClearItem(InItem);

	CopyAllItem(Inventory.GetSaveDataRef<FInventorySaveData>(true));
}

void FInventorySaveData::ClearAllItem()
{
	UAbilityInventoryBase& Inventory = UReferencePoolModuleStatics::GetReference<UAbilityInventoryBase>(false, InventoryClass);

	Inventory.LoadSaveData(this);
	Inventory.ClearAllItem();

	CopyAllItem(Inventory.GetSaveDataRef<FInventorySaveData>(true));
}

UAbilityItemDataBase& FActorSaveData::GetItemData() const
{
	return UAssetModuleStatics::LoadPrimaryAssetRef<UAbilityItemDataBase>(AssetID);
}
