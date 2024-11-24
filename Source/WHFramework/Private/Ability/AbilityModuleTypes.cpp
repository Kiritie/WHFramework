#include "Ability/AbilityModuleTypes.h"

#include "Ability/Actor/AbilityActorDataBase.h"
#include "Asset/AssetModuleStatics.h"
#include "Ability/Item/AbilityItemDataBase.h"
#include "Ability/Character/AbilityCharacterBase.h"
#include "Ability/Inventory/AbilityInventoryBase.h"
#include "Ability/Inventory/Slot/AbilityInventorySlotBase.h"
#include "Ability/Vitality/AbilityVitalityInterface.h"
#include "Common/CommonStatics.h"

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

void UDamageHandle::HandleDamage(AActor* SourceActor, AActor* TargetActor, float DamageValue, const FGameplayAttribute& DamageAttribute, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags)
{
	IAbilityVitalityInterface* TargetVitality = Cast<IAbilityVitalityInterface>(TargetActor);

	if (DamageValue > 0.f)
	{
		if (TargetVitality && !TargetVitality->IsDead())
		{
			TargetVitality->HandleDamage(DamageAttribute, DamageValue, false, false, HitResult, SourceTags, SourceActor);
		}
	}
}

void URecoveryHandle::HandleRecovery(AActor* SourceActor, AActor* TargetActor, float RecoveryValue, const FGameplayAttribute& RecoveryAttribute, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags)
{
	IAbilityVitalityInterface* TargetVitality = Cast<IAbilityVitalityInterface>(TargetActor);

	if (RecoveryValue > 0.f)
	{
		if (TargetVitality && !TargetVitality->IsDead())
		{
			TargetVitality->HandleRecovery(RecoveryAttribute, RecoveryValue, HitResult, SourceTags, SourceActor);
		}
	}
}

void UInterruptHandle::HandleInterrupt(AActor* SourceActor, AActor* TargetActor, float InterruptDuration, const FGameplayAttribute& InterruptAttribute, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags)
{
	IAbilityVitalityInterface* TargetVitality = Cast<IAbilityVitalityInterface>(TargetActor);

	if (InterruptDuration > 0.f)
	{
		if (TargetVitality && !TargetVitality->IsDead())
		{
			TargetVitality->HandleInterrupt(InterruptAttribute, InterruptDuration, HitResult, SourceTags, SourceActor);
		}
	}
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

UPrimaryAssetBase& FAbilityData::GetData() const
{
	return UAssetModuleStatics::LoadPrimaryAssetRef<UPrimaryAssetBase>(ID);
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

void FInventorySaveData::FillItems(int32 InLevel, FRandomStream InRandomStream)
{
	if(InRandomStream.GetInitialSeed() == 0) InRandomStream = FRandomStream(FMath::Rand());
	
	for(auto& Iter1 : FillRules)
	{
		if(Iter1.Key == EAbilityItemType::None) continue;

		const FName ItemType = *UCommonStatics::GetEnumValueAuthoredName(TEXT("/Script/WHFramework.EAbilityItemType"), (int32)Iter1.Key);

		int32 ItemNum = 0;
		EAbilityItemRarity ItemParity = EAbilityItemRarity::None;
		const float ParityRate = InRandomStream.FRand();
		for(auto& Iter2 : Iter1.Value.Rules)
		{
			if(ParityRate <= Iter2.Value.Rate)
			{
				ItemParity = Iter2.Key;
				const float ItemRate = InRandomStream.FRand();
				for(auto& Iter3 : Iter2.Value.Items)
				{
					if(ItemRate < Iter3.Rate)
					{
						ItemNum = Iter3.Num;
						break;
					}
				}
				break;
			}
		}

		TArray<UAbilityItemDataBase*> ItemDatas;
		for(auto Iter : UAssetModuleStatics::LoadPrimaryAssets<UAbilityItemDataBase>(ItemType))
		{
			if(Iter->Rarity == ItemParity)
			{
				ItemDatas.Add(Iter);
			}
		}

		if(Iter1.Key == EAbilityItemType::Coin)
		{
			if (ItemDatas.IsValidIndex(0))
			{
				AddItem(FAbilityItem(ItemDatas[0]->GetPrimaryAssetId(), ItemNum, ItemDatas[0]->ClampLevel(InLevel)));
			}
		}
		else
		{
			for (int32 i = 0; i < ItemNum; i++)
			{
				if(ItemDatas.IsEmpty()) break;

				const int32 Index = FMath::RandRange(0, ItemDatas.Num() - 1);
				AddItem(FAbilityItem(ItemDatas[Index]->GetPrimaryAssetId(), 1, ItemDatas[Index]->ClampLevel(InLevel)));
				ItemDatas.RemoveAt(Index);
			}
		}
	}
}

void FInventorySaveData::CopyItems(const FInventorySaveData& InSaveData)
{
	SplitItems = InSaveData.SplitItems;
}

void FInventorySaveData::AddItem(FAbilityItem InItem, bool bUnique)
{
	UAbilityInventoryBase& Inventory = UReferencePoolModuleStatics::GetReference<UAbilityInventoryBase>(true, InventoryClass);

	Inventory.LoadSaveData(this);
	if(!bUnique || !Inventory.QueryItemByRange(EItemQueryType::Get, InItem).IsValid())
	{
		Inventory.AddItemByRange(InItem, 0, -1, false);
	}

	CopyItems(Inventory.GetSaveDataRef<FInventorySaveData>(true));
}

void FInventorySaveData::RemoveItem(FAbilityItem InItem)
{
	UAbilityInventoryBase& Inventory = UReferencePoolModuleStatics::GetReference<UAbilityInventoryBase>(true, InventoryClass);

	Inventory.LoadSaveData(this);
	Inventory.RemoveItemByRange(InItem, 0, -1, false);

	CopyItems(Inventory.GetSaveDataRef<FInventorySaveData>(true));
}

void FInventorySaveData::ClearItem(FAbilityItem InItem)
{
	UAbilityInventoryBase& Inventory = UReferencePoolModuleStatics::GetReference<UAbilityInventoryBase>(true, InventoryClass);

	Inventory.LoadSaveData(this);
	Inventory.ClearItem(InItem);

	CopyItems(Inventory.GetSaveDataRef<FInventorySaveData>(true));
}

void FInventorySaveData::ClearItems()
{
	UAbilityInventoryBase& Inventory = UReferencePoolModuleStatics::GetReference<UAbilityInventoryBase>(true, InventoryClass);

	Inventory.LoadSaveData(this);
	Inventory.ClearItems();

	CopyItems(Inventory.GetSaveDataRef<FInventorySaveData>(true));
}

UAbilityItemDataBase& FRaceItem::GetData() const
{
	return UAssetModuleStatics::LoadPrimaryAssetRef<UAbilityItemDataBase>(ID);
}

void FActorSaveData::InitInventoryData(FRandomStream InRandomStream)
{
	InventoryData = GetData().InventoryData;
	InventoryData.FillItems(Level, InRandomStream);
}

UAbilityActorDataBase& FActorSaveData::GetData() const
{
	return UAssetModuleStatics::LoadPrimaryAssetRef<UAbilityActorDataBase>(AssetID);
}
