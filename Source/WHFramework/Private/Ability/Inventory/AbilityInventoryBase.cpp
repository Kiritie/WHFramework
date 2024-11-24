// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Ability/Inventory/AbilityInventoryBase.h"

#include "Ability/Inventory/AbilityInventoryAgentInterface.h"
#include "Ability/Inventory/Slot/AbilityInventorySlotBase.h"
#include "Ability/Inventory/Slot/AbilityInventoryAuxiliarySlotBase.h"
#include "Ability/Inventory/Slot/AbilityInventoryEquipSlotBase.h"
#include "Ability/Inventory/Slot/AbilityInventoryShortcutSlotBase.h"
#include "Ability/Inventory/Slot/AbilityInventorySkillSlotBase.h"
#include "Common/CommonStatics.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"

UAbilityInventoryBase::UAbilityInventoryBase()
{
	SplitSlots = TMap<ESlotSplitType, FInventorySlots>();
	SelectedIndexs = TMap<ESlotSplitType, int32>();
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
		UObjectPoolModuleStatics::DespawnObjects(Iter1.Value.Slots);
	}
	SplitSlots.Empty();
	SelectedIndexs.Empty();
	ConnectInventory = nullptr;
	OnSlotSelected.Clear();
}

void UAbilityInventoryBase::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	auto& SaveData = InSaveData->CastRef<FInventorySaveData>();

	if(PHASEC(InPhase, EPhase::Primary))
	{
		for (auto& Iter : SaveData.SplitItems)
		{
			for (int32 i = 0; i < Iter.Value.Items.Num(); i++)
			{
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
						Slot = UObjectPoolModuleStatics::SpawnObject<UAbilityInventoryShortcutSlotBase>(nullptr, nullptr, ShortcutSlotClass);
						Slot->OnInitialize(this, EAbilityItemType::None, Iter.Key, i);
						break;
					}
					case ESlotSplitType::Auxiliary:
					{
						Slot = UObjectPoolModuleStatics::SpawnObject<UAbilityInventoryAuxiliarySlotBase>(nullptr, nullptr, AuxiliarySlotClass);
						Slot->OnInitialize(this, EAbilityItemType::None, Iter.Key, i);
						break;
					}
					case ESlotSplitType::Equip:
					{
						Slot = UObjectPoolModuleStatics::SpawnObject<UAbilityInventoryEquipSlotBase>(nullptr, nullptr, EquipSlotClass);
						Slot->OnInitialize(this, EAbilityItemType::Equip, Iter.Key, i);
						break;
					}
					case ESlotSplitType::Skill:
					{
						Slot = UObjectPoolModuleStatics::SpawnObject<UAbilityInventorySkillSlotBase>(nullptr, nullptr, SkillSlotClass);
						Slot->OnInitialize(this, EAbilityItemType::Skill, Iter.Key, i);
						break;
					}
					default: break;
				}
				SplitSlots.FindOrAdd(Iter.Key).Slots.Add(Slot);
			}
		}
	}

	if(PHASEC(InPhase, EPhase::All))
	{
		for (auto& Iter : SaveData.SplitItems)
		{
			for (int32 i = 0; i < Iter.Value.Items.Num(); i++)
			{
				FAbilityItem& Item = Iter.Value.Items[i];
				UAbilityInventorySlotBase* Slot = SplitSlots.FindOrAdd(Iter.Key).Slots[i];
				Slot->SetItem(Item);
				if(SaveData.SelectedIndexs.Contains(Iter.Key) ? SaveData.SelectedIndexs[Iter.Key] == i : i == 0)
				{
					SetSelectedSlot(Iter.Key, i);
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
	SaveData.SelectedIndexs = SelectedIndexs;
	return &SaveData;
}

void UAbilityInventoryBase::UnloadData(EPhase InPhase)
{
	for(auto& Iter1 : SplitSlots)
	{
		UObjectPoolModuleStatics::DespawnObjects(Iter1.Value.Slots);
	}
	SplitSlots.Empty();
	SelectedIndexs.Empty();
	ConnectInventory = nullptr;
	OnSlotSelected.Clear();
}

FItemQueryData UAbilityInventoryBase::QueryItemByRange(EItemQueryType InQueryType, FAbilityItem InItem, int32 InStartIndex, int32 InEndIndex)
{
	TArray<UAbilityInventorySlotBase*> Slots = GetAllSlots();
	if (!InItem.IsValid() || Slots.IsEmpty()) return FItemQueryData();

	int32 StartIndex;
	if (InStartIndex == -1) StartIndex = GetSelectedSlot(ESlotSplitType::Shortcut) ? Slots.IndexOfByKey(GetSelectedSlot(ESlotSplitType::Shortcut)) : 0;
	else StartIndex = FMath::Clamp(InStartIndex, 0, Slots.Num() - 1);
	if(InEndIndex != -1) InEndIndex = FMath::Clamp(InEndIndex, InStartIndex, Slots.Num() - 1);

	FItemQueryData QueryData;
	#define ITERATION1(Index, Expression) \
	for (int32 Index = (InStartIndex == -1 ? 0 : StartIndex); i <= (InEndIndex == -1 ? (Slots.Num() - 1) : InEndIndex); i++) \
	{ \
		Expression \
	}
	#define EXPRESSION1(Index) \
	if (Slots[Index]->ContainsItem(InItem)) \
	{ \
		if (!QueryData.Slots.Contains(Slots[Index])) \
		{ \
			QueryData.Slots.Add(Slots[Index]); \
			QueryData.Item.Count += Slots[Index]->GetItem().Count; \
			if (InItem.Count > 0 && QueryData.Item.Count >= InItem.Count) goto END; \
		} \
	}
	#define EXPRESSION2(Index, bPutIn) \
	if (Slots[Index]->MatchItem(InItem, bPutIn)) \
	{ \
		if (!QueryData.Slots.Contains(Slots[Index])) \
		{ \
			QueryData.Slots.Add(Slots[Index]); \
			InItem.Count -= Slots[Index]->GetRemainVolume(InItem); \
			if (InItem.Count <= 0) goto END; \
		} \
	}
	#define EXPRESSION3(bCheckSplit, bPutIn, bContains) \
	ITERATION1(i , \
		if ((!bCheckSplit || Slots[i]->MatchItemSplit(InItem, true)) && (!bContains || Slots[i]->ContainsItem(InItem))) \
		{ \
			EXPRESSION2(i, bPutIn) \
		} \
	)
	#define EXPRESSION4(Index) \
	if (Slots[Index]->ContainsItem(InItem)) \
	{ \
		if (!QueryData.Slots.Contains(Slots[Index])) \
		{ \
			QueryData.Slots.Add(Slots[Index]); \
			InItem.Count -= Slots[Index]->GetItem().Count; \
			if (InItem.Count <= 0) goto END; \
		} \
	}
	
	switch (InQueryType)
	{
		case EItemQueryType::Get:
		{
			QueryData.Item = FAbilityItem(InItem, 0);
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
			QueryData.Item = InItem;
			if (InItem.Count <= 0) goto END;
			EXPRESSION3(true, true, false)
			if (InStartIndex == -1)
			{
				EXPRESSION2(StartIndex, true)
			}
			EXPRESSION3(false, true, true)
			EXPRESSION3(false, true, false)
			break;
		}
		case EItemQueryType::Remove:
		{
			QueryData.Item = InItem;
			if (InItem.Count <= 0) goto END;
			if (InStartIndex == -1)
			{
				EXPRESSION4(StartIndex)
			}
			ITERATION1(i ,
				EXPRESSION4(i)
			)
			break;
		}
		case EItemQueryType::Match:
		{
			QueryData.Item = InItem;
			if (InItem.Count <= 0) goto END;
			EXPRESSION3(true, false, false)
			break;
		}
	}
	END:
	if(InItem.Count > 0)
	{
		if(InQueryType != EItemQueryType::Get) QueryData.Item.Count -= InItem.Count;
		else QueryData.Item.Count = FMath::Clamp(QueryData.Item.Count, 0, InItem.Count);
	}
	return QueryData;
}

FItemQueryData UAbilityInventoryBase::QueryItemBySplitType(EItemQueryType InQueryType, FAbilityItem InItem, ESlotSplitType InSplitType)
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

FItemQueryData UAbilityInventoryBase::QueryItemBySplitTypes(EItemQueryType InQueryType, FAbilityItem InItem, const TArray<ESlotSplitType>& InSplitTypes)
{
	FItemQueryData QueryData;
	for(auto& Iter : InSplitTypes)
	{
		QueryData += QueryItemBySplitType(InQueryType, InItem, Iter);
		InItem.Count -= QueryData.Item.Count;
		if(InItem.Count <= 0) break;
	}
	return QueryData;
}

void UAbilityInventoryBase::AddItemByRange(FAbilityItem& InItem, int32 InStartIndex, int32 InEndIndex, bool bBroadcast)
{
	const auto QueryData = QueryItemByRange(EItemQueryType::Add, InItem, InStartIndex, InEndIndex);
	AddItemBySlots(InItem, QueryData.Slots, bBroadcast);
}

void UAbilityInventoryBase::AddItemBySplitType(FAbilityItem& InItem, ESlotSplitType InSplitType, bool bBroadcast)
{
	const auto QueryData = QueryItemBySplitType(EItemQueryType::Add, InItem, InSplitType);
	AddItemBySlots(InItem, QueryData.Slots, bBroadcast);
}

void UAbilityInventoryBase::AddItemBySplitTypes(FAbilityItem& InItem, const TArray<ESlotSplitType>& InSplitTypes, bool bBroadcast)
{
	const auto QueryData = QueryItemBySplitTypes(EItemQueryType::Add, InItem, InSplitTypes);
	AddItemBySlots(InItem, QueryData.Slots, bBroadcast);
}

void UAbilityInventoryBase::AddItemByQueryData(FItemQueryData& InQueryData, bool bBroadcast)
{
	AddItemBySlots(InQueryData.Item, InQueryData.Slots, bBroadcast);
}

void UAbilityInventoryBase::RemoveItemByRange(FAbilityItem& InItem, int32 InStartIndex, int32 InEndIndex, bool bBroadcast)
{
	const auto QueryData = QueryItemByRange(EItemQueryType::Remove, InItem, InStartIndex, InEndIndex);
	RemoveItemBySlots(InItem, QueryData.Slots, bBroadcast);
}

void UAbilityInventoryBase::RemoveItemBySplitType(FAbilityItem& InItem, ESlotSplitType InSplitType, bool bBroadcast)
{
	const auto QueryData = QueryItemBySplitType(EItemQueryType::Remove, InItem, InSplitType);
	RemoveItemBySlots(InItem, QueryData.Slots, bBroadcast);
}

void UAbilityInventoryBase::RemoveItemBySplitTypes(FAbilityItem& InItem, const TArray<ESlotSplitType>& InSplitTypes, bool bBroadcast)
{
	const auto QueryData = QueryItemBySplitTypes(EItemQueryType::Remove, InItem, InSplitTypes);
	RemoveItemBySlots(InItem, QueryData.Slots, bBroadcast);
}

void UAbilityInventoryBase::RemoveItemByQueryData(FItemQueryData& InQueryData, bool bBroadcast)
{
	RemoveItemBySlots(InQueryData.Item, InQueryData.Slots, bBroadcast);
}

void UAbilityInventoryBase::MoveItemByRange(UAbilityInventoryBase* InTargetInventory, FAbilityItem& InItem, int32 InSelfStartIndex, int32 InSelfEndIndex, int32 InTargetStartIndex, int32 InTargetEndIndex)
{
	const auto QueryData = InTargetInventory->QueryItemByRange(EItemQueryType::Remove, InItem, InTargetStartIndex, InTargetEndIndex);
	InItem.Count = InItem.Count != -1 ? FMath::Min(InItem.Count, QueryData.Item.Count) : QueryData.Item.Count;
	FAbilityItem _Item = FAbilityItem(InItem);
	RemoveItemByRange(_Item, InSelfStartIndex, InSelfEndIndex);
	InItem -= _Item;
	InTargetInventory->AddItemByRange(InItem, InTargetStartIndex, InTargetEndIndex);
}

void UAbilityInventoryBase::MoveItemBySplitType(UAbilityInventoryBase* InTargetInventory, FAbilityItem& InItem, ESlotSplitType InSelfSlotSplitType, ESlotSplitType InTargetSlotSplitType)
{
	const auto QueryData = InTargetInventory->QueryItemBySplitType(EItemQueryType::Remove, InItem, InTargetSlotSplitType);
	InItem.Count = InItem.Count != -1 ? FMath::Min(InItem.Count, QueryData.Item.Count) : QueryData.Item.Count;
	FAbilityItem _Item = FAbilityItem(InItem);
	RemoveItemBySplitType(_Item, InSelfSlotSplitType);
	InItem -= _Item;
	InTargetInventory->AddItemBySplitType(InItem, InTargetSlotSplitType);
}

void UAbilityInventoryBase::ClearItem(FAbilityItem InItem)
{
	auto QueryData = QueryItemByRange(EItemQueryType::Get, InItem);
	for (int i = 0; i < QueryData.Slots.Num(); i++)
	{
		QueryData.Slots[i]->ClearItem();
	}
}

void UAbilityInventoryBase::ClearItems()
{
	for(auto& Iter1 : SplitSlots)
	{
		for(auto& Iter2 : Iter1.Value.Slots)
		{
			Iter2->ClearItem();
		}
	}
}

void UAbilityInventoryBase::DiscardItems()
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
	auto _Item = InItem;
	if(InItem.GetType() != EAbilityItemType::Misc)
	{
		for(int i = 0; i < InSlots.Num(); i++)
		{
			InSlots[i]->AddItem(InItem);
			if (InItem.Count <= 0) break;
		}
		_Item.Count -= InItem.Count;
	}
	if(auto Agent = GetOwnerAgent())
	{
		if(bBroadcast) Agent->OnAdditionItem(_Item);
	}
}

void UAbilityInventoryBase::RemoveItemBySlots(FAbilityItem& InItem, const TArray<UAbilityInventorySlotBase*>& InSlots, bool bBroadcast)
{
	auto _Item = InItem;
	if(InItem.GetType() != EAbilityItemType::Misc)
	{
		for (int i = 0; i < InSlots.Num(); i++)
		{
			InSlots[i]->SubItem(InItem);
			if (InItem.Count <= 0) break;
		}
		_Item.Count -= InItem.Count;
	}
	if(auto Agent = GetOwnerAgent())
	{
		if(bBroadcast) Agent->OnRemoveItem(_Item);
	}
}

TScriptInterface<IAbilityInventoryAgentInterface> UAbilityInventoryBase::GetOwnerAgent() const
{
	return GetOuter();
}

UAbilityInventorySlotBase* UAbilityInventoryBase::GetSelectedSlot(ESlotSplitType InSplitType) const
{
	if(SelectedIndexs.Contains(InSplitType))
	{
		return GetSlotBySplitTypeAndIndex(InSplitType, SelectedIndexs[InSplitType]);
	}
	return nullptr;
}

void UAbilityInventoryBase::SetSelectedSlot(ESlotSplitType InSplitType, int32 InSlotIndex)
{
	if(!SelectedIndexs.Contains(InSplitType) || SelectedIndexs[InSplitType] != InSlotIndex)
	{
		SelectedIndexs.Emplace(InSplitType, InSlotIndex);
		if(UAbilityInventorySlotBase* SelectedSlot = GetSelectedSlot(InSplitType))
		{
			OnSlotSelected.Broadcast(SelectedSlot);
			if(auto Agent = GetOwnerAgent())
			{
				Agent->OnSelectItem(SelectedSlot->GetItem());
			}
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

TArray<UAbilityInventorySlotBase*> UAbilityInventoryBase::GetAllSlots() const
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

TArray<FAbilityItem> UAbilityInventoryBase::GetAllItems() const
{
	TArray<FAbilityItem> Items;
	for(auto Iter1 : GetAllSlots())
	{
		bool bMatched = false;
		for(auto& Iter2 : Items)
		{
			if(Iter2.Match(Iter1->GetItem()))
			{
				Iter2 += Iter1->GetItem();
				bMatched = true;
				break;
			}
		}
		if(!bMatched)
		{
			Items.Add(Iter1->GetItem());
		}
	}
	return Items;
}

TMap<EAbilityItemType, FAbilityItems> UAbilityInventoryBase::GetAllItemMap() const
{
	TMap<EAbilityItemType, FAbilityItems> ItemMap;
	for(int32 i = 1; i < UCommonStatics::GetEnumItemNum(TEXT("/Script/WHFramework.EAbilityItemType")); i++)
	{
		ItemMap.Add((EAbilityItemType)i);
	}
	for(auto Iter1 : GetAllSlots())
	{
		bool bMatched = false;
		FAbilityItems& Items = ItemMap[Iter1->GetItem().GetType()];
		for(auto& Iter2 : Items.Items)
		{
			if(Iter2.Match(Iter1->GetItem()))
			{
				Iter2 += Iter1->GetItem();
				bMatched = true;
				break;
			}
		}
		if(!bMatched)
		{
			Items.Items.Add(Iter1->GetItem());
		}
	}
	return ItemMap;
}

TArray<UAbilityInventorySlotBase*> UAbilityInventoryBase::GetSlotsBySplitType(ESlotSplitType InSplitType) const
{
	if(SplitSlots.Contains(InSplitType))
	{
		return SplitSlots[InSplitType].Slots;
	}
	return TArray<UAbilityInventorySlotBase*>();
}

UAbilityInventorySlotBase* UAbilityInventoryBase::GetSlotBySplitTypeAndIndex(ESlotSplitType InSplitType, int32 InIndex) const
{
	TArray<UAbilityInventorySlotBase*> Slots = GetSlotsBySplitType(InSplitType);
	if(Slots.IsValidIndex(InIndex))
	{
		return Slots[InIndex];
	}
	return nullptr;
}

UAbilityInventorySlotBase* UAbilityInventoryBase::GetSlotBySplitTypeAndItemID(ESlotSplitType InSplitType, const FPrimaryAssetId& InItemID) const
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
