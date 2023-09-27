// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Ability/Inventory/AbilityInventoryBase.h"

#include "Ability/Inventory/AbilityInventoryAgentInterface.h"
#include "Ability/Inventory/Slot/AbilityInventorySlot.h"
#include "Ability/Inventory/Slot/AbilityInventoryAuxiliarySlot.h"
#include "Ability/Inventory/Slot/AbilityInventoryEquipSlot.h"
#include "Ability/Inventory/Slot/AbilityInventoryShortcutSlot.h"
#include "Ability/Inventory/Slot/AbilityInventorySkillSlot.h"
#include "ObjectPool/ObjectPoolModuleBPLibrary.h"

UAbilityInventoryBase::UAbilityInventoryBase()
{
	SplitSlots = TMap<ESlotSplitType, FInventorySlots>();
	ConnectInventory = nullptr;
	SelectedSlot = nullptr;

	ShortcutSlotClass = UAbilityInventoryShortcutSlot::StaticClass();
	AuxiliarySlotClass = UAbilityInventoryAuxiliarySlot::StaticClass();
	EquipSlotClass = UAbilityInventoryEquipSlot::StaticClass();
	SkillSlotClass = UAbilityInventorySkillSlot::StaticClass();
}

void UAbilityInventoryBase::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	auto& SaveData = InSaveData->CastRef<FInventorySaveData>();

	if(PHASEC(InPhase, EPhase::Primary))
	{
		for(auto& Iter1 : SplitSlots)
		{
			for(auto& Iter2 : Iter1.Value.Slots)
			{
				UObjectPoolModuleBPLibrary::DespawnObject(Iter2);
			}
		}
		SplitSlots.Empty();
	}
	if(PHASEC(InPhase, EPhase::All))
	{
		for (auto& Iter : SaveData.SplitItems)
		{
			for (int32 i = 0; i < Iter.Value.Items.Num(); i++)
			{
				FAbilityItem& Item = Iter.Value.Items[i];
				UAbilityInventorySlot* Slot = GetSlotBySplitTypeAndIndex(Iter.Key, i);
				if(Slot) Slot->SetItem(Item);
				switch(Iter.Key)
				{
					case ESlotSplitType::Default:
					{
						if(!Slot)
						{
							Slot = UObjectPoolModuleBPLibrary::SpawnObject<UAbilityInventorySlot>();
							Slot->OnInitialize(this, Item, EAbilityItemType::None, Iter.Key);
						}
						break;
					}
					case ESlotSplitType::Shortcut:
					{
						if(!Slot)
						{
							Slot = UObjectPoolModuleBPLibrary::SpawnObject<UAbilityInventoryShortcutSlot>(nullptr, ShortcutSlotClass);
							Slot->OnInitialize(this, Item, EAbilityItemType::None, Iter.Key);
						}
						if(i == SaveData.SelectedIndex)
						{
							SetSelectedSlot(Slot);
						}
						break;
					}
					case ESlotSplitType::Auxiliary:
					{
						if(!Slot)
						{
							Slot = UObjectPoolModuleBPLibrary::SpawnObject<UAbilityInventoryAuxiliarySlot>(nullptr, AuxiliarySlotClass);
							Slot->OnInitialize(this, Item, EAbilityItemType::None, Iter.Key);
						}
						break;
					}
					case ESlotSplitType::Equip:
					{
						if(!Slot)
						{
							Slot = UObjectPoolModuleBPLibrary::SpawnObject<UAbilityInventoryEquipSlot>(nullptr, EquipSlotClass);
							Cast<UAbilityInventoryEquipSlot>(Slot)->OnInitialize(this, Item, EAbilityItemType::Equip, Iter.Key, i);
						}
						break;
					}
					case ESlotSplitType::Skill:
					{
						if(!Slot)
						{
							Slot = UObjectPoolModuleBPLibrary::SpawnObject<UAbilityInventorySkillSlot>(nullptr, SkillSlotClass);
							Slot->OnInitialize(this, Item, EAbilityItemType::Skill, Iter.Key);
						}
						break;
					}
					default: break;
				}
				SplitSlots.FindOrAdd(Iter.Key).Slots.EmplaceAt(i, Slot);
			}
		}
	}
}

FSaveData* UAbilityInventoryBase::ToData(bool bRefresh)
{
	static FInventorySaveData SaveData;
	SaveData = FInventorySaveData();

	for(auto& Iter : SplitSlots)
	{
		SaveData.SplitItems.Add(Iter.Key, Iter.Value.GetItems());
	}
	if(SelectedSlot)
	{
		SaveData.SelectedIndex = SelectedSlot->GetSplitIndex();
	}
	return &SaveData;
}

void UAbilityInventoryBase::UnloadData(EPhase InPhase)
{
	SplitSlots.Empty();
	for(auto& Iter1 : SplitSlots)
	{
		for(auto& Iter2 : Iter1.Value.Slots)
		{
			UObjectPoolModuleBPLibrary::DespawnObject(Iter2);
		}
	}
	SplitSlots.Empty();
	ConnectInventory = nullptr;
	SelectedSlot = nullptr;
	OnSlotSelected.Clear();
}

FItemQueryInfo UAbilityInventoryBase::QueryItemByRange(EItemQueryType InQueryType, FAbilityItem InItem, int32 InStartIndex, int32 InEndIndex)
{
	TArray<UAbilityInventorySlot*> Slots = GetSlots();
	if (!InItem.IsValid() || Slots.IsEmpty()) return FItemQueryInfo();

	int32 StartIndex;
	if (InStartIndex == -1) StartIndex = GetSelectedSlot() ? Slots.IndexOfByKey(GetSelectedSlot()) : 0;
	else StartIndex = FMath::Clamp(InStartIndex, 0, Slots.Num() - 1);
	if(InEndIndex != -1) InEndIndex = FMath::Clamp(InEndIndex, InStartIndex, Slots.Num() - 1);

	FItemQueryInfo ItemQueryInfo;
	switch (InQueryType)
	{
		case EItemQueryType::Get:
		{
			ItemQueryInfo.Item = FAbilityItem(InItem, 0);
			if (InStartIndex == -1 && Slots[StartIndex]->Contains(InItem))
			{
				if (!ItemQueryInfo.Slots.Contains(Slots[StartIndex]))
				{
					ItemQueryInfo.Slots.Add(Slots[StartIndex]);
					ItemQueryInfo.Item.Count += Slots[StartIndex]->GetItem().Count;
					if (InItem.Count > 0 && ItemQueryInfo.Item.Count >= InItem.Count) goto End;
				}
			}
			for (int32 i = (InStartIndex == -1 ? 0 : StartIndex); i <= (InEndIndex == -1 ? (Slots.Num() - 1) : InEndIndex); i++)
			{
				if (Slots[i]->Contains(InItem))
				{
					if (!ItemQueryInfo.Slots.Contains(Slots[i]))
					{
						ItemQueryInfo.Slots.Add(Slots[i]);
						ItemQueryInfo.Item.Count += Slots[i]->GetItem().Count;
						if (InItem.Count > 0 && ItemQueryInfo.Item.Count >= InItem.Count) goto End;
					}
				}
			}
			break;
		}
		case EItemQueryType::Add:
		{
			ItemQueryInfo.Item = InItem;
			if (InItem.Count <= 0) goto End;
			#define Query1(bNeedMatch, bNeedContains) \
			for (int32 i = (InStartIndex == -1 ? 0 : StartIndex); i <= (InEndIndex == -1 ? (Slots.Num() - 1) : InEndIndex); i++) \
			{ \
				if (Slots[i]->CanPutIn(InItem) && (!bNeedMatch || Slots[i]->IsMatch(InItem, true)) && (!bNeedContains || Slots[i]->Contains(InItem))) \
				{ \
					if (!ItemQueryInfo.Slots.Contains(Slots[i])) \
					{ \
						ItemQueryInfo.Slots.Add(Slots[i]); \
						InItem.Count -= Slots[i]->GetRemainVolume(InItem); \
						if (InItem.Count <= 0) goto End; \
					} \
				} \
			}
			Query1(true, false)
			if (InStartIndex == -1 && Slots[StartIndex]->CanPutIn(InItem))
			{
				if (!ItemQueryInfo.Slots.Contains(Slots[StartIndex]))
				{
					ItemQueryInfo.Slots.Add(Slots[StartIndex]);
					InItem.Count -= Slots[StartIndex]->GetRemainVolume(InItem);
					if (InItem.Count <= 0) goto End;
				}
			}
			Query1(false, true)
			Query1(false, false)
			break;
		}
		case EItemQueryType::Remove:
		{
			ItemQueryInfo.Item = InItem;
			if (InItem.Count <= 0) goto End;
			if (InStartIndex == -1 && Slots[StartIndex]->Contains(InItem))
			{
				if (!ItemQueryInfo.Slots.Contains(Slots[StartIndex]))
				{
					ItemQueryInfo.Slots.Add(Slots[StartIndex]);
					InItem.Count -= Slots[StartIndex]->GetItem().Count;
					if (InItem.Count <= 0) goto End;
				}
			}
			for (int32 i = (InStartIndex == -1 ? 0 : StartIndex); i <= (InEndIndex == -1 ? (Slots.Num() - 1) : InEndIndex); i++)
			{
				if (Slots[i]->Contains(InItem))
				{
					if (!ItemQueryInfo.Slots.Contains(Slots[i]))
					{
						ItemQueryInfo.Slots.Add(Slots[i]);
						InItem.Count -= Slots[i]->GetItem().Count;
						if (InItem.Count <= 0) goto End;
					}
				}
			}
			break;
		}
	}
	End:
	if(InItem.Count > 0)
	{
		if(InQueryType != EItemQueryType::Get)
		{
			ItemQueryInfo.Item.Count -= InItem.Count;
		}
		else
		{
			ItemQueryInfo.Item.Count = FMath::Clamp(ItemQueryInfo.Item.Count, 0, InItem.Count);
		}
	}
	return ItemQueryInfo;
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
	FItemQueryInfo ItemQueryInfo;
	for(auto& Iter : InSplitTypes)
	{
		ItemQueryInfo += QueryItemBySplitType(InQueryType, InItem, Iter);
		InItem.Count -= ItemQueryInfo.Item.Count;
		if(InItem.Count <= 0) break;
	}
	return ItemQueryInfo;
}

void UAbilityInventoryBase::AddItemBySlots(FAbilityItem& InItem, const TArray<UAbilityInventorySlot*>& InSlots)
{
	for (int i = 0; i < InSlots.Num(); i++)
	{
		InSlots[i]->AddItem(InItem);
		if (InItem.Count <= 0) break;
	}
}

void UAbilityInventoryBase::AddItemByRange(FAbilityItem& InItem, int32 InStartIndex, int32 InEndIndex)
{
	const auto ItemQueryInfo = QueryItemByRange(EItemQueryType::Add, InItem, InStartIndex, InEndIndex);
	AddItemBySlots(InItem, ItemQueryInfo.Slots);
}

void UAbilityInventoryBase::AddItemBySplitType(FAbilityItem& InItem, ESlotSplitType InSplitType)
{
	const auto ItemQueryInfo = QueryItemBySplitType(EItemQueryType::Add, InItem, InSplitType);
	AddItemBySlots(InItem, ItemQueryInfo.Slots);
}

void UAbilityInventoryBase::AddItemBySplitTypes(FAbilityItem& InItem, const TArray<ESlotSplitType>& InSplitTypes)
{
	const auto ItemQueryInfo = QueryItemBySplitTypes(EItemQueryType::Add, InItem, InSplitTypes);
	AddItemBySlots(InItem, ItemQueryInfo.Slots);
}

void UAbilityInventoryBase::AddItemByQueryInfo(FItemQueryInfo& InQueryInfo)
{
	AddItemBySlots(InQueryInfo.Item, InQueryInfo.Slots);
}

void UAbilityInventoryBase::RemoveItemBySlots(FAbilityItem& InItem, const TArray<UAbilityInventorySlot*>& InSlots)
{
	for (int i = 0; i < InSlots.Num(); i++)
	{
		InSlots[i]->SubItem(InItem);
		if (InItem.Count <= 0) break;
	}
}

void UAbilityInventoryBase::RemoveItemByRange(FAbilityItem& InItem, int32 InStartIndex, int32 InEndIndex)
{
	const auto ItemQueryInfo = QueryItemByRange(EItemQueryType::Remove, InItem, InStartIndex, InEndIndex);
	RemoveItemBySlots(InItem, ItemQueryInfo.Slots);
}

void UAbilityInventoryBase::RemoveItemBySplitType(FAbilityItem& InItem, ESlotSplitType InSplitType)
{
	const auto ItemQueryInfo = QueryItemBySplitType(EItemQueryType::Remove, InItem, InSplitType);
	RemoveItemBySlots(InItem, ItemQueryInfo.Slots);
}

void UAbilityInventoryBase::RemoveItemBySplitTypes(FAbilityItem& InItem, const TArray<ESlotSplitType>& InSplitTypes)
{
	const auto ItemQueryInfo = QueryItemBySplitTypes(EItemQueryType::Remove, InItem, InSplitTypes);
	RemoveItemBySlots(InItem, ItemQueryInfo.Slots);
}

void UAbilityInventoryBase::RemoveItemByQueryInfo(FItemQueryInfo& InQueryInfo)
{
	RemoveItemBySlots(InQueryInfo.Item, InQueryInfo.Slots);
}

void UAbilityInventoryBase::MoveItemByRange(UAbilityInventoryBase* InTargetInventory, FAbilityItem& InItem, int32 InSelfStartIndex, int32 InSelfEndIndex, int32 InTargetStartIndex, int32 InTargetEndIndex)
{
	const auto ItemQueryInfo = InTargetInventory->QueryItemByRange(EItemQueryType::Remove, InItem, InTargetStartIndex, InTargetEndIndex);
	InItem.Count = InItem.Count != -1 ? FMath::Min(InItem.Count, ItemQueryInfo.Item.Count) : ItemQueryInfo.Item.Count;
	FAbilityItem tmpItem = FAbilityItem(InItem);
	RemoveItemByRange(tmpItem, InSelfStartIndex, InSelfEndIndex);
	InItem -= tmpItem;
	InTargetInventory->AddItemByRange(InItem, InTargetStartIndex, InTargetEndIndex);
}

void UAbilityInventoryBase::MoveItemBySplitType(UAbilityInventoryBase* InTargetInventory, FAbilityItem& InItem, ESlotSplitType InSelfSlotSplitType, ESlotSplitType InTargetSlotSplitType)
{
	const auto ItemQueryInfo = InTargetInventory->QueryItemBySplitType(EItemQueryType::Remove, InItem, InTargetSlotSplitType);
	InItem.Count = InItem.Count != -1 ? FMath::Min(InItem.Count, ItemQueryInfo.Item.Count) : ItemQueryInfo.Item.Count;
	FAbilityItem tmpItem = FAbilityItem(InItem);
	RemoveItemBySplitType(tmpItem, InSelfSlotSplitType);
	InItem -= tmpItem;
	InTargetInventory->AddItemBySplitType(InItem, InTargetSlotSplitType);
}

void UAbilityInventoryBase::ClearItem(FAbilityItem InItem)
{
	auto ItemQueryInfo = QueryItemByRange(EItemQueryType::Get, InItem);
	for (int i = 0; i < ItemQueryInfo.Slots.Num(); i++)
	{
		ItemQueryInfo.Slots[i]->ClearItem();
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

TScriptInterface<IAbilityInventoryAgentInterface> UAbilityInventoryBase::GetOwnerAgent() const
{
	return GetOuter();
}

void UAbilityInventoryBase::SetSelectedSlot(UAbilityInventorySlot* InSelectedSlot)
{
	SelectedSlot = InSelectedSlot;
	OnSlotSelected.Broadcast(InSelectedSlot);
	if(auto Agent = GetOwnerAgent())
	{
		Agent->OnSelectItem(InSelectedSlot->GetItem());
	}
}

FAbilityItem& UAbilityInventoryBase::GetSelectedItem() const
{
	if(GetSelectedSlot())
	{
		return GetSelectedSlot()->GetItem();
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

TArray<UAbilityInventorySlot*> UAbilityInventoryBase::GetSlots()
{
	TArray<UAbilityInventorySlot*> Slots;
	for(auto& Iter1 : SplitSlots)
	{
		for(auto& Iter2 : Iter1.Value.Slots)
		{
			Slots.Add(Iter2);
		}
	}
	return Slots;
}

TArray<UAbilityInventorySlot*> UAbilityInventoryBase::GetSlotsBySplitType(ESlotSplitType InSplitType)
{
	if(SplitSlots.Contains(InSplitType))
	{
		return SplitSlots[InSplitType].Slots;
	}
	return TArray<UAbilityInventorySlot*>();
}

UAbilityInventorySlot* UAbilityInventoryBase::GetSlotBySplitTypeAndIndex(ESlotSplitType InSplitType, int32 InIndex)
{
	TArray<UAbilityInventorySlot*> Slots = GetSlotsBySplitType(InSplitType);
	if(Slots.IsValidIndex(InIndex))
	{
		return Slots[InIndex];
	}
	return nullptr;
}

UAbilityInventorySlot* UAbilityInventoryBase::GetSlotBySplitTypeAndItemID(ESlotSplitType InSplitType, const FPrimaryAssetId& InItemID)
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
