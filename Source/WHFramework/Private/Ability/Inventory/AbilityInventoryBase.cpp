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
	SplitInfos = TMap<ESplitSlotType, FSplitSlotInfo>();
	Slots = TArray<UAbilityInventorySlot*>();
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
		for(auto Iter : Slots)
		{
			UObjectPoolModuleBPLibrary::DespawnObject(Iter);
		}
		Slots.Empty();
		SplitInfos = SaveData.SplitInfos;
		for (auto Iter : SplitInfos)
		{
			for (int32 i = Iter.Value.StartIndex; i < Iter.Value.StartIndex + Iter.Value.TotalCount; i++)
			{
				UAbilityInventorySlot* Slot = nullptr;
				switch(Iter.Key)
				{
					case ESplitSlotType::Default:
					{
						Slot = UObjectPoolModuleBPLibrary::SpawnObject<UAbilityInventorySlot>();
						Slot->OnInitialize(this, FAbilityItem::Empty, EAbilityItemType::None, Iter.Key);
						break;
					}
					case ESplitSlotType::Shortcut:
					{
						Slot = UObjectPoolModuleBPLibrary::SpawnObject<UAbilityInventoryShortcutSlot>(nullptr, ShortcutSlotClass);
						Slot->OnInitialize(this, FAbilityItem::Empty, EAbilityItemType::None, Iter.Key);
						break;
					}
					case ESplitSlotType::Auxiliary:
					{
						Slot = UObjectPoolModuleBPLibrary::SpawnObject<UAbilityInventoryAuxiliarySlot>(nullptr, AuxiliarySlotClass);
						Slot->OnInitialize(this, FAbilityItem::Empty, EAbilityItemType::None, Iter.Key);
						break;
					}
					case ESplitSlotType::Equip:
					{
						Slot = UObjectPoolModuleBPLibrary::SpawnObject<UAbilityInventoryEquipSlot>(nullptr, EquipSlotClass);
						Cast<UAbilityInventoryEquipSlot>(Slot)->OnInitialize(this, FAbilityItem::Empty, EAbilityItemType::Equip, Iter.Key, i - Iter.Value.StartIndex);
						break;
					}
					case ESplitSlotType::Skill:
					{
						Slot = UObjectPoolModuleBPLibrary::SpawnObject<UAbilityInventorySkillSlot>(nullptr, SkillSlotClass);
						Slot->OnInitialize(this, FAbilityItem::Empty, EAbilityItemType::Skill, Iter.Key);
						break;
					}
					default: break;
				}
				Slots.Add(Slot);
			}
		}
	}
	if(PHASEC(InPhase, EPhase::All))
	{
		for (int32 i = 0; i < Slots.Num(); i++)
		{
			Slots[i]->SetItem(SaveData.Items.IsValidIndex(i) ? SaveData.Items[i] : FAbilityItem::Empty);
			if(i == SaveData.SelectedIndex)
			{
				SetSelectedSlot(Slots[i]);
			}
		}
	}
}

FSaveData* UAbilityInventoryBase::ToData(bool bRefresh)
{
	static FInventorySaveData SaveData;
	SaveData = FInventorySaveData();

	SaveData.SplitInfos = SplitInfos;
	if(SelectedSlot)
	{
		SaveData.SelectedIndex = SelectedSlot->GetSplitIndex();
	}

	for (int32 i = 0; i < Slots.Num(); i++)
	{
		SaveData.Items.Add(Slots[i]->GetItem());
	}
	return &SaveData;
}

void UAbilityInventoryBase::UnloadData(EPhase InPhase)
{
	SplitInfos.Empty();
	for(auto Iter : Slots)
	{
		UObjectPoolModuleBPLibrary::DespawnObject(Iter);
	}
	Slots.Empty();
	ConnectInventory = nullptr;
	SelectedSlot = nullptr;
	OnSlotSelected.Clear();
}

FQueryItemInfo UAbilityInventoryBase::QueryItemByRange(EQueryItemType InQueryType, FAbilityItem InItem, int32 InStartIndex, int32 InEndIndex)
{
	if (!InItem.IsValid() || Slots.IsEmpty()) return FQueryItemInfo();

	int32 StartIndex;
	if (InStartIndex == -1) StartIndex = GetSelectedSlot() ? Slots.IndexOfByKey(GetSelectedSlot()) : 0;
	else StartIndex = FMath::Clamp(InStartIndex, 0, Slots.Num() - 1);
	if(InEndIndex != -1) InEndIndex = FMath::Clamp(InEndIndex, InStartIndex, Slots.Num() - 1);

	FQueryItemInfo QueryItemInfo;
	switch (InQueryType)
	{
		case EQueryItemType::Get:
		{
			QueryItemInfo.Item = FAbilityItem(InItem, 0);
			if (InStartIndex == -1 && Slots[StartIndex]->Contains(InItem))
			{
				if (!QueryItemInfo.Slots.Contains(Slots[StartIndex]))
				{
					QueryItemInfo.Slots.Add(Slots[StartIndex]);
					QueryItemInfo.Item.Count += Slots[StartIndex]->GetItem().Count;
					if (InItem.Count > 0 && QueryItemInfo.Item.Count >= InItem.Count) goto End;
				}
			}
			for (int32 i = (InStartIndex == -1 ? 0 : StartIndex); i <= (InEndIndex == -1 ? (Slots.Num() - 1) : InEndIndex); i++)
			{
				if (Slots[i]->Contains(InItem))
				{
					if (!QueryItemInfo.Slots.Contains(Slots[i]))
					{
						QueryItemInfo.Slots.Add(Slots[i]);
						QueryItemInfo.Item.Count += Slots[i]->GetItem().Count;
						if (InItem.Count > 0 && QueryItemInfo.Item.Count >= InItem.Count) goto End;
					}
				}
			}
			break;
		}
		case EQueryItemType::Add:
		{
			QueryItemInfo.Item = InItem;
			if (InItem.Count <= 0) goto End;
			#define Query1(bNeedMatch, bNeedContains) \
			for (int32 i = (InStartIndex == -1 ? 0 : StartIndex); i <= (InEndIndex == -1 ? (Slots.Num() - 1) : InEndIndex); i++) \
			{ \
				if (Slots[i]->CanPutIn(InItem) && (!bNeedMatch || Slots[i]->IsMatch(InItem, true)) && (!bNeedContains || Slots[i]->Contains(InItem))) \
				{ \
					if (!QueryItemInfo.Slots.Contains(Slots[i])) \
					{ \
						QueryItemInfo.Slots.Add(Slots[i]); \
						InItem.Count -= Slots[i]->GetRemainVolume(InItem); \
						if (InItem.Count <= 0) goto End; \
					} \
				} \
			}
			Query1(true, false)
			if (InStartIndex == -1 && Slots[StartIndex]->CanPutIn(InItem))
			{
				if (!QueryItemInfo.Slots.Contains(Slots[StartIndex]))
				{
					QueryItemInfo.Slots.Add(Slots[StartIndex]);
					InItem.Count -= Slots[StartIndex]->GetRemainVolume(InItem);
					if (InItem.Count <= 0) goto End;
				}
			}
			Query1(false, true)
			Query1(false, false)
			break;
		}
		case EQueryItemType::Remove:
		{
			QueryItemInfo.Item = InItem;
			if (InItem.Count <= 0) goto End;
			if (InStartIndex == -1 && Slots[StartIndex]->Contains(InItem))
			{
				if (!QueryItemInfo.Slots.Contains(Slots[StartIndex]))
				{
					QueryItemInfo.Slots.Add(Slots[StartIndex]);
					InItem.Count -= Slots[StartIndex]->GetItem().Count;
					if (InItem.Count <= 0) goto End;
				}
			}
			for (int32 i = (InStartIndex == -1 ? 0 : StartIndex); i <= (InEndIndex == -1 ? (Slots.Num() - 1) : InEndIndex); i++)
			{
				if (Slots[i]->Contains(InItem))
				{
					if (!QueryItemInfo.Slots.Contains(Slots[i]))
					{
						QueryItemInfo.Slots.Add(Slots[i]);
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
		if(InQueryType != EQueryItemType::Get)
		{
			QueryItemInfo.Item.Count -= InItem.Count;
		}
		else
		{
			QueryItemInfo.Item.Count = FMath::Clamp(QueryItemInfo.Item.Count, 0, InItem.Count);
		}
	}
	return QueryItemInfo;
}

FQueryItemInfo UAbilityInventoryBase::QueryItemBySplitType(EQueryItemType InQueryType, FAbilityItem InItem, ESplitSlotType InSplitSlotType)
{
	const FSplitSlotInfo SplitSlotInfo = GetSplitSlotInfo(InSplitSlotType);
	return QueryItemByRange(InQueryType, InItem, SplitSlotInfo.StartIndex, SplitSlotInfo.StartIndex + SplitSlotInfo.TotalCount - 1);
}

FQueryItemInfo UAbilityInventoryBase::QueryItemBySplitTypes(EQueryItemType InQueryType, FAbilityItem InItem, const TArray<ESplitSlotType>& InSplitSlotTypes)
{
	FQueryItemInfo QueryItemInfo;
	for(auto Iter : InSplitSlotTypes)
	{
		QueryItemInfo += QueryItemBySplitType(InQueryType, InItem, Iter);
		InItem.Count -= QueryItemInfo.Item.Count;
		if(InItem.Count <= 0) break;
	}
	return QueryItemInfo;
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
	const auto QueryItemInfo = QueryItemByRange(EQueryItemType::Add, InItem, InStartIndex, InEndIndex);
	AddItemBySlots(InItem, QueryItemInfo.Slots);
}

void UAbilityInventoryBase::AddItemBySplitType(FAbilityItem& InItem, ESplitSlotType InSplitSlotType)
{
	const auto QueryItemInfo = QueryItemBySplitType(EQueryItemType::Add, InItem, InSplitSlotType);
	AddItemBySlots(InItem, QueryItemInfo.Slots);
}

void UAbilityInventoryBase::AddItemBySplitTypes(FAbilityItem& InItem, const TArray<ESplitSlotType>& InSplitSlotTypes)
{
	const auto QueryItemInfo = QueryItemBySplitTypes(EQueryItemType::Add, InItem, InSplitSlotTypes);
	AddItemBySlots(InItem, QueryItemInfo.Slots);
}

void UAbilityInventoryBase::AddItemByQueryInfo(FQueryItemInfo& InQueryInfo)
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
	const auto QueryItemInfo = QueryItemByRange(EQueryItemType::Remove, InItem, InStartIndex, InEndIndex);
	RemoveItemBySlots(InItem, QueryItemInfo.Slots);
}

void UAbilityInventoryBase::RemoveItemBySplitType(FAbilityItem& InItem, ESplitSlotType InSplitSlotType)
{
	const auto QueryItemInfo = QueryItemBySplitType(EQueryItemType::Remove, InItem, InSplitSlotType);
	RemoveItemBySlots(InItem, QueryItemInfo.Slots);
}

void UAbilityInventoryBase::RemoveItemBySplitTypes(FAbilityItem& InItem, const TArray<ESplitSlotType>& InSplitSlotTypes)
{
	const auto QueryItemInfo = QueryItemBySplitTypes(EQueryItemType::Remove, InItem, InSplitSlotTypes);
	RemoveItemBySlots(InItem, QueryItemInfo.Slots);
}

void UAbilityInventoryBase::RemoveItemByQueryInfo(FQueryItemInfo& InQueryInfo)
{
	RemoveItemBySlots(InQueryInfo.Item, InQueryInfo.Slots);
}

void UAbilityInventoryBase::MoveItemByRange(UAbilityInventoryBase* InTargetInventory, FAbilityItem& InItem, int32 InSelfStartIndex, int32 InSelfEndIndex, int32 InTargetStartIndex, int32 InTargetEndIndex)
{
	const auto QueryItemInfo = InTargetInventory->QueryItemByRange(EQueryItemType::Remove, InItem, InTargetStartIndex, InTargetEndIndex);
	InItem.Count = InItem.Count != -1 ? FMath::Min(InItem.Count, QueryItemInfo.Item.Count) : QueryItemInfo.Item.Count;
	FAbilityItem tmpItem = FAbilityItem(InItem);
	RemoveItemByRange(tmpItem, InSelfStartIndex, InSelfEndIndex);
	InItem -= tmpItem;
	InTargetInventory->AddItemByRange(InItem, InTargetStartIndex, InTargetEndIndex);
}

void UAbilityInventoryBase::MoveItemBySplitType(UAbilityInventoryBase* InTargetInventory, FAbilityItem& InItem, ESplitSlotType InSelfSplitSlotType, ESplitSlotType InTargetSplitSlotType)
{
	const auto QueryItemInfo = InTargetInventory->QueryItemBySplitType(EQueryItemType::Remove, InItem, InTargetSplitSlotType);
	InItem.Count = InItem.Count != -1 ? FMath::Min(InItem.Count, QueryItemInfo.Item.Count) : QueryItemInfo.Item.Count;
	FAbilityItem tmpItem = FAbilityItem(InItem);
	RemoveItemBySplitType(tmpItem, InSelfSplitSlotType);
	InItem -= tmpItem;
	InTargetInventory->AddItemBySplitType(InItem, InTargetSplitSlotType);
}

void UAbilityInventoryBase::ClearItem(FAbilityItem InItem)
{
	auto QueryItemInfo = QueryItemByRange(EQueryItemType::Get, InItem);
	for (int i = 0; i < QueryItemInfo.Slots.Num(); i++)
	{
		QueryItemInfo.Slots[i]->ClearItem();
	}
}

void UAbilityInventoryBase::ClearAllItem()
{
	for (int i = 0; i < Slots.Num(); i++)
	{
		Slots[i]->ClearItem();
	}
}

void UAbilityInventoryBase::DiscardAllItem()
{
	for (int i = 0; i < Slots.Num(); i++)
	{
		Slots[i]->DiscardItem(-1, true);
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

int32 UAbilityInventoryBase::GetSlotsNum() const
{
	return Slots.Num();
}

bool UAbilityInventoryBase::HasSplitSlotInfo(ESplitSlotType InSplitSlotType) const
{
	return SplitInfos.Contains(InSplitSlotType);
}

FSplitSlotInfo UAbilityInventoryBase::GetSplitSlotInfo(ESplitSlotType InSplitSlotType) const
{
	if(InSplitSlotType != ESplitSlotType::None)
	{
		if(HasSplitSlotInfo(InSplitSlotType))
		{
			return SplitInfos[InSplitSlotType];
		}
	}
	else
	{
		return FSplitSlotInfo(0, GetSlotsNum() - 1);
	}
	return FSplitSlotInfo();
}

TArray<UAbilityInventorySlot*> UAbilityInventoryBase::GetSplitSlots(ESplitSlotType InSplitSlotType)
{
	if(InSplitSlotType == ESplitSlotType::None) return Slots;
	
	TArray<UAbilityInventorySlot*> SplitSlots = TArray<UAbilityInventorySlot*>();

	for (auto Iter : SplitInfos)
	{
		if (Iter.Key == InSplitSlotType)
		{
			for (int32 i = Iter.Value.StartIndex; i < Iter.Value.StartIndex + Iter.Value.TotalCount; i++)
			{
				SplitSlots.Add(Slots[i]);
			}
			break;
		}
	}

	return SplitSlots;
}

TMap<ESplitSlotType, FSplitSlotData> UAbilityInventoryBase::GetSplitSlotDatas()
{
	TMap<ESplitSlotType, FSplitSlotData> SplitSlotDatas = TMap<ESplitSlotType, FSplitSlotData>();

	for (auto Iter : SplitInfos)
	{
		FSplitSlotData SplitSlotData = FSplitSlotData();
		for (int32 i = Iter.Value.StartIndex; i < Iter.Value.StartIndex + Iter.Value.TotalCount; i++)
		{
			SplitSlotData.Slots.Add(Slots[i]);
		}
		SplitSlotDatas.Add(Iter.Key, SplitSlotData);
	}

	return SplitSlotDatas;
}
