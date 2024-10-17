// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Ability/Inventory/AbilityInventoryBase.h"

#include "Ability/Inventory/AbilityInventoryAgentInterface.h"
#include "Ability/Inventory/Slot/AbilityInventorySlotBase.h"
#include "Ability/Inventory/Slot/AbilityInventoryAuxiliarySlotBase.h"
#include "Ability/Inventory/Slot/AbilityInventoryEquipSlotBase.h"
#include "Ability/Inventory/Slot/AbilityInventoryShortcutSlotBase.h"
#include "Ability/Inventory/Slot/AbilityInventorySkillSlotBase.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"

UAbilityInventoryBase::UAbilityInventoryBase()
{
	SplitSlots = TMap<ESlotSplitType, FInventorySlots>();
	SelectedSlots = TMap<ESlotSplitType, UAbilityInventorySlotBase*>();
	ConnectInventory = nullptr;

	ShortcutSlotClass = UAbilityInventoryShortcutSlotBase::StaticClass();
	AuxiliarySlotClass = UAbilityInventoryAuxiliarySlotBase::StaticClass();
	EquipSlotClass = UAbilityInventoryEquipSlotBase::StaticClass();
	SkillSlotClass = UAbilityInventorySkillSlotBase::StaticClass();
}

void UAbilityInventoryBase::OnReset_Implementation()
{
	for(auto& Iter1 : SplitSlots)
	{
		for(const auto& Iter2 : Iter1.Value.Slots)
		{
			UObjectPoolModuleStatics::DespawnObject(Iter2);
		}
	}
	SplitSlots.Empty();
	SelectedSlots.Empty();
	ConnectInventory = nullptr;
	OnSlotSelected.Clear();
}

void UAbilityInventoryBase::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	auto& SaveData = InSaveData->CastRef<FInventorySaveData>();

	if(PHASEC(InPhase, EPhase::All))
	{
		for(auto& Iter1 : SplitSlots)
		{
			for(const auto& Iter2 : Iter1.Value.Slots)
			{
				UObjectPoolModuleStatics::DespawnObject(Iter2);
			}
		}
		SplitSlots.Empty();
		SelectedSlots.Empty();
		for (auto& Iter : SaveData.SplitItems)
		{
			for (int32 i = 0; i < Iter.Value.Items.Num(); i++)
			{
				FAbilityItem& Item = Iter.Value.Items[i];
				UAbilityInventorySlotBase* Slot = nullptr;
				switch(Iter.Key)
				{
					case ESlotSplitType::Default:
					{
						Slot = UObjectPoolModuleStatics::SpawnObject<UAbilityInventorySlotBase>();
						Slot->OnInitialize(this, EAbilityItemType::None, Iter.Key, i);
						break;
					}
					case ESlotSplitType::Shortcut:
					{
						Slot = UObjectPoolModuleStatics::SpawnObject<UAbilityInventoryShortcutSlotBase>(nullptr, nullptr, false, ShortcutSlotClass);
						Slot->OnInitialize(this, EAbilityItemType::None, Iter.Key, i);
						break;
					}
					case ESlotSplitType::Auxiliary:
					{
						Slot = UObjectPoolModuleStatics::SpawnObject<UAbilityInventoryAuxiliarySlotBase>(nullptr, nullptr, false, AuxiliarySlotClass);
						Slot->OnInitialize(this, EAbilityItemType::None, Iter.Key, i);
						break;
					}
					case ESlotSplitType::Equip:
					{
						Slot = UObjectPoolModuleStatics::SpawnObject<UAbilityInventoryEquipSlotBase>(nullptr, nullptr, false, EquipSlotClass);
						Slot->OnInitialize(this, EAbilityItemType::Equip, Iter.Key, i);
						break;
					}
					case ESlotSplitType::Skill:
					{
						Slot = UObjectPoolModuleStatics::SpawnObject<UAbilityInventorySkillSlotBase>(nullptr, nullptr, false, SkillSlotClass);
						Slot->OnInitialize(this, EAbilityItemType::Skill, Iter.Key, i);
						break;
					}
					default: break;
				}
				SplitSlots.FindOrAdd(Iter.Key).Slots.Add(Slot);
				Slot->SetItem(Item);
				if(SaveData.SelectedIndexs.Contains(Iter.Key) && SaveData.SelectedIndexs[Iter.Key] == i)
				{
					SetSelectedSlot(Iter.Key, Slot);
				}
			}
		}
	}
}

FSaveData* UAbilityInventoryBase::ToData()
{
	static FInventorySaveData SaveData;
	SaveData = FInventorySaveData();

	SaveData.InventoryClass = GetClass();
	for(auto& Iter : SplitSlots)
	{
		SaveData.SplitItems.Add(Iter.Key, Iter.Value.GetItems());
	}
	for(auto& Iter : SelectedSlots)
	{
		SaveData.SelectedIndexs.Add(Iter.Key, Iter.Value->GetSlotIndex());
	}
	return &SaveData;
}

void UAbilityInventoryBase::UnloadData(EPhase InPhase)
{
	for(auto& Iter1 : SplitSlots)
	{
		for(const auto& Iter2 : Iter1.Value.Slots)
		{
			UObjectPoolModuleStatics::DespawnObject(Iter2);
		}
	}
	SplitSlots.Empty();
	SelectedSlots.Empty();
	ConnectInventory = nullptr;
	OnSlotSelected.Clear();
}

FItemQueryInfo UAbilityInventoryBase::QueryItemByRange(EItemQueryType InQueryType, FAbilityItem InItem, int32 InStartIndex, int32 InEndIndex)
{
	TArray<UAbilityInventorySlotBase*> Slots = GetSlots();
	if (!InItem.IsValid() || Slots.IsEmpty()) return FItemQueryInfo();

	int32 StartIndex;
	if (InStartIndex == -1) StartIndex = GetSelectedSlot(ESlotSplitType::Shortcut) ? Slots.IndexOfByKey(GetSelectedSlot(ESlotSplitType::Shortcut)) : 0;
	else StartIndex = FMath::Clamp(InStartIndex, 0, Slots.Num() - 1);
	if(InEndIndex != -1) InEndIndex = FMath::Clamp(InEndIndex, InStartIndex, Slots.Num() - 1);

	FItemQueryInfo QueryInfo;
	#define ITERATION1(Index, Expression) \
	for (int32 Index = (InStartIndex == -1 ? 0 : StartIndex); i <= (InEndIndex == -1 ? (Slots.Num() - 1) : InEndIndex); i++) \
	{ \
		Expression \
	}

	switch (InQueryType)
	{
		case EItemQueryType::Get:
		{
			QueryInfo.Item = FAbilityItem(InItem, 0);
			#define EXPRESSION1(Index) \
			if (Slots[Index]->Contains(InItem)) \
			{ \
				if (!QueryInfo.Slots.Contains(Slots[Index])) \
				{ \
					QueryInfo.Slots.Add(Slots[Index]); \
					QueryInfo.Item.Count += Slots[Index]->GetItem().Count; \
					if (InItem.Count > 0 && QueryInfo.Item.Count >= InItem.Count) goto END; \
				} \
			}
			if (InStartIndex == -1)
			{
				EXPRESSION1(StartIndex)
			}
			ITERATION1(i ,
				EXPRESSION1(i)
			)
			break;
		}
		case EItemQueryType::Add:
		{
			QueryInfo.Item = InItem;
			if (InItem.Count <= 0) goto END;
			#define EXPRESSION2(Index) \
			if (Slots[Index]->CanPutIn(InItem)) \
			{ \
				if (!QueryInfo.Slots.Contains(Slots[Index])) \
				{ \
					QueryInfo.Slots.Add(Slots[Index]); \
					InItem.Count -= Slots[Index]->GetRemainVolume(InItem); \
					if (InItem.Count <= 0) goto END; \
				} \
			}
			#define EXPRESSION3(bNeedMatch, bNeedContains) \
			ITERATION1(i , \
				if ((!bNeedMatch || Slots[i]->IsMatch(InItem, true)) && (!bNeedContains || Slots[i]->Contains(InItem))) \
				{ \
					EXPRESSION2(i) \
				} \
			)
			EXPRESSION3(true, false)
			if (InStartIndex == -1)
			{
				EXPRESSION2(StartIndex)
			}
			EXPRESSION3(false, true)
			EXPRESSION3(false, false)
			break;
		}
		case EItemQueryType::Remove:
		{
			QueryInfo.Item = InItem;
			if (InItem.Count <= 0) goto END;
			#define EXPRESSION4(Index) \
			if (Slots[Index]->Contains(InItem)) \
			{ \
				if (!QueryInfo.Slots.Contains(Slots[Index])) \
				{ \
					QueryInfo.Slots.Add(Slots[Index]); \
					InItem.Count -= Slots[Index]->GetItem().Count; \
					if (InItem.Count <= 0) goto END; \
				} \
			}
			if (InStartIndex == -1)
			{
				EXPRESSION4(StartIndex)
			}
			ITERATION1(i ,
				EXPRESSION4(i)
			)
			break;
		}
	}
	END:
	if(InItem.Count > 0)
	{
		if(InQueryType != EItemQueryType::Get) QueryInfo.Item.Count -= InItem.Count;
		else QueryInfo.Item.Count = FMath::Clamp(QueryInfo.Item.Count, 0, InItem.Count);
	}
	return QueryInfo;
}

FItemQueryInfo UAbilityInventoryBase::QueryItemBySplitType(EItemQueryType InQueryType, FAbilityItem InItem, ESlotSplitType InSplitType)
{
	int32 StartIndex = 0;
	int32 EndIndex = 0;
	for(auto& Iter : SplitSlots)
	{
		if(Iter.Key == InSplitType)
		{
			EndIndex = StartIndex + Iter.Value.Slots.Num();
			break;
		}
		StartIndex += Iter.Value.Slots.Num();
	}
	return QueryItemByRange(InQueryType, InItem, StartIndex, EndIndex);
}

FItemQueryInfo UAbilityInventoryBase::QueryItemBySplitTypes(EItemQueryType InQueryType, FAbilityItem InItem, const TArray<ESlotSplitType>& InSplitTypes)
{
	FItemQueryInfo QueryInfo;
	for(auto& Iter : InSplitTypes)
	{
		QueryInfo += QueryItemBySplitType(InQueryType, InItem, Iter);
		InItem.Count -= QueryInfo.Item.Count;
		if(InItem.Count <= 0) break;
	}
	return QueryInfo;
}

void UAbilityInventoryBase::AddItemByRange(FAbilityItem& InItem, int32 InStartIndex, int32 InEndIndex, bool bBroadcast)
{
	const auto QueryInfo = QueryItemByRange(EItemQueryType::Add, InItem, InStartIndex, InEndIndex);
	AddItemBySlots(InItem, QueryInfo.Slots, bBroadcast);
}

void UAbilityInventoryBase::AddItemBySplitType(FAbilityItem& InItem, ESlotSplitType InSplitType, bool bBroadcast)
{
	const auto QueryInfo = QueryItemBySplitType(EItemQueryType::Add, InItem, InSplitType);
	AddItemBySlots(InItem, QueryInfo.Slots, bBroadcast);
}

void UAbilityInventoryBase::AddItemBySplitTypes(FAbilityItem& InItem, const TArray<ESlotSplitType>& InSplitTypes, bool bBroadcast)
{
	const auto QueryInfo = QueryItemBySplitTypes(EItemQueryType::Add, InItem, InSplitTypes);
	AddItemBySlots(InItem, QueryInfo.Slots, bBroadcast);
}

void UAbilityInventoryBase::AddItemByQueryInfo(FItemQueryInfo& InQueryInfo, bool bBroadcast)
{
	AddItemBySlots(InQueryInfo.Item, InQueryInfo.Slots, bBroadcast);
}

void UAbilityInventoryBase::RemoveItemByRange(FAbilityItem& InItem, int32 InStartIndex, int32 InEndIndex, bool bBroadcast)
{
	const auto QueryInfo = QueryItemByRange(EItemQueryType::Remove, InItem, InStartIndex, InEndIndex);
	RemoveItemBySlots(InItem, QueryInfo.Slots, bBroadcast);
}

void UAbilityInventoryBase::RemoveItemBySplitType(FAbilityItem& InItem, ESlotSplitType InSplitType, bool bBroadcast)
{
	const auto QueryInfo = QueryItemBySplitType(EItemQueryType::Remove, InItem, InSplitType);
	RemoveItemBySlots(InItem, QueryInfo.Slots, bBroadcast);
}

void UAbilityInventoryBase::RemoveItemBySplitTypes(FAbilityItem& InItem, const TArray<ESlotSplitType>& InSplitTypes, bool bBroadcast)
{
	const auto QueryInfo = QueryItemBySplitTypes(EItemQueryType::Remove, InItem, InSplitTypes);
	RemoveItemBySlots(InItem, QueryInfo.Slots, bBroadcast);
}

void UAbilityInventoryBase::RemoveItemByQueryInfo(FItemQueryInfo& InQueryInfo, bool bBroadcast)
{
	RemoveItemBySlots(InQueryInfo.Item, InQueryInfo.Slots, bBroadcast);
}

void UAbilityInventoryBase::MoveItemByRange(UAbilityInventoryBase* InTargetInventory, FAbilityItem& InItem, int32 InSelfStartIndex, int32 InSelfEndIndex, int32 InTargetStartIndex, int32 InTargetEndIndex)
{
	const auto QueryInfo = InTargetInventory->QueryItemByRange(EItemQueryType::Remove, InItem, InTargetStartIndex, InTargetEndIndex);
	InItem.Count = InItem.Count != -1 ? FMath::Min(InItem.Count, QueryInfo.Item.Count) : QueryInfo.Item.Count;
	FAbilityItem tmpItem = FAbilityItem(InItem);
	RemoveItemByRange(tmpItem, InSelfStartIndex, InSelfEndIndex);
	InItem -= tmpItem;
	InTargetInventory->AddItemByRange(InItem, InTargetStartIndex, InTargetEndIndex);
}

void UAbilityInventoryBase::MoveItemBySplitType(UAbilityInventoryBase* InTargetInventory, FAbilityItem& InItem, ESlotSplitType InSelfSlotSplitType, ESlotSplitType InTargetSlotSplitType)
{
	const auto QueryInfo = InTargetInventory->QueryItemBySplitType(EItemQueryType::Remove, InItem, InTargetSlotSplitType);
	InItem.Count = InItem.Count != -1 ? FMath::Min(InItem.Count, QueryInfo.Item.Count) : QueryInfo.Item.Count;
	FAbilityItem tmpItem = FAbilityItem(InItem);
	RemoveItemBySplitType(tmpItem, InSelfSlotSplitType);
	InItem -= tmpItem;
	InTargetInventory->AddItemBySplitType(InItem, InTargetSlotSplitType);
}

void UAbilityInventoryBase::ClearItem(FAbilityItem InItem)
{
	auto QueryInfo = QueryItemByRange(EItemQueryType::Get, InItem);
	for (int i = 0; i < QueryInfo.Slots.Num(); i++)
	{
		QueryInfo.Slots[i]->ClearItem();
	}
}

void UAbilityInventoryBase::ClearAllItem()
{
	for(auto& Iter1 : SplitSlots)
	{
		for(auto& Iter2 : Iter1.Value.Slots)
		{
			Iter2->ClearItem();
		}
	}
}

void UAbilityInventoryBase::DiscardAllItem()
{
	for(auto& Iter1 : SplitSlots)
	{
		for(auto& Iter2 : Iter1.Value.Slots)
		{
			Iter2->DiscardItem(-1, true);
		}
	}
}

void UAbilityInventoryBase::AddItemBySlots(FAbilityItem& InItem, const TArray<UAbilityInventorySlotBase*>& InSlots, bool bBroadcast)
{
	auto tmpItem = InItem;
	for(int i = 0; i < InSlots.Num(); i++)
	{
		InSlots[i]->AddItem(InItem);
		if (InItem.Count <= 0) break;
	}
	tmpItem.Count -= InItem.Count;
	if(auto Agent = GetOwnerAgent())
	{
		if(bBroadcast) Agent->OnAdditionItem(tmpItem);
	}
}

void UAbilityInventoryBase::RemoveItemBySlots(FAbilityItem& InItem, const TArray<UAbilityInventorySlotBase*>& InSlots, bool bBroadcast)
{
	auto tmpItem = InItem;
	for (int i = 0; i < InSlots.Num(); i++)
	{
		InSlots[i]->SubItem(InItem);
		if (InItem.Count <= 0) break;
	}
	tmpItem.Count -= InItem.Count;
	if(auto Agent = GetOwnerAgent())
	{
		if(bBroadcast) Agent->OnRemoveItem(tmpItem);
	}
}

TScriptInterface<IAbilityInventoryAgentInterface> UAbilityInventoryBase::GetOwnerAgent() const
{
	return GetOuter();
}

UAbilityInventorySlotBase* UAbilityInventoryBase::GetSelectedSlot(ESlotSplitType InSplitType) const
{
	if(SelectedSlots.Contains(InSplitType))
	{
		return SelectedSlots[InSplitType];
	}
	return nullptr;
}

void UAbilityInventoryBase::SetSelectedSlot(ESlotSplitType InSplitType, UAbilityInventorySlotBase* InSelectedSlot)
{
	if(!SelectedSlots.Contains(InSplitType) || SelectedSlots[InSplitType] != InSelectedSlot)
	{
		SelectedSlots.Emplace(InSplitType, InSelectedSlot);
		OnSlotSelected.Broadcast(InSelectedSlot);
		if(auto Agent = GetOwnerAgent())
		{
			Agent->OnSelectItem(InSplitType, InSelectedSlot->GetItem());
		}
	}
}

FAbilityItem& UAbilityInventoryBase::GetSelectedItem(ESlotSplitType InSplitType) const
{
	if(GetSelectedSlot(InSplitType))
	{
		return GetSelectedSlot(InSplitType)->GetItem();
	}
	return FAbilityItem::Empty;
}

void UAbilityInventoryBase::SetConnectInventory(UAbilityInventoryBase* InInventory)
{
	if(InInventory)
	{
		InInventory->ConnectInventory = this;
	}
	else if(ConnectInventory)
	{
		ConnectInventory->ConnectInventory = nullptr;
	}
	ConnectInventory = InInventory;
}

bool UAbilityInventoryBase::HasSplitType(ESlotSplitType InSplitType) const
{
	return SplitSlots.Contains(InSplitType);
}

TArray<UAbilityInventorySlotBase*> UAbilityInventoryBase::GetSlots()
{
	TArray<UAbilityInventorySlotBase*> Slots;
	for(auto& Iter1 : SplitSlots)
	{
		for(auto& Iter2 : Iter1.Value.Slots)
		{
			Slots.Add(Iter2);
		}
	}
	return Slots;
}

TArray<UAbilityInventorySlotBase*> UAbilityInventoryBase::GetSlotsBySplitType(ESlotSplitType InSplitType)
{
	if(SplitSlots.Contains(InSplitType))
	{
		return SplitSlots[InSplitType].Slots;
	}
	return TArray<UAbilityInventorySlotBase*>();
}

UAbilityInventorySlotBase* UAbilityInventoryBase::GetSlotBySplitTypeAndIndex(ESlotSplitType InSplitType, int32 InIndex)
{
	TArray<UAbilityInventorySlotBase*> Slots = GetSlotsBySplitType(InSplitType);
	if(Slots.IsValidIndex(InIndex))
	{
		return Slots[InIndex];
	}
	return nullptr;
}

UAbilityInventorySlotBase* UAbilityInventoryBase::GetSlotBySplitTypeAndItemID(ESlotSplitType InSplitType, const FPrimaryAssetId& InItemID)
{
	auto Slots = GetSlotsBySplitType(InSplitType);
	for(const auto Iter : Slots)
	{
		if(Iter->GetItem().ID == InItemID)
		{
			return Iter;
		}
	}
	return nullptr;
}
