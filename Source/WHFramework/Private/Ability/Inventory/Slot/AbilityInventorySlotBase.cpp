// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Ability/Inventory/Slot/AbilityInventorySlotBase.h"

#include "Ability/AbilityModuleStatics.h"
#include "Ability/Abilities/ItemAbilityBase.h"
#include "Ability/Actor/AbilityActorInterface.h"
#include "Ability/Components/AbilitySystemComponentBase.h"
#include "Ability/Item/AbilityItemDataBase.h"
#include "Ability/Inventory/AbilityInventoryBase.h"
#include "Ability/Inventory/AbilityInventoryAgentInterface.h"
#include "Voxel/VoxelModule.h"

UAbilityInventorySlotBase::UAbilityInventorySlotBase()
{
	Item = FAbilityItem::Empty;
	Item.InventorySlot = this;
	Inventory = nullptr;
	LimitType = EAbilityItemType::None;
	SplitType = ESlotSplitType::Default;
	SlotIndex = 0;
}

void UAbilityInventorySlotBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	
}

void UAbilityInventorySlotBase::OnDespawn_Implementation(bool bRecovery)
{
	SetItem(FAbilityItem::Empty);
	Inventory = nullptr;
	LimitType = EAbilityItemType::None;
	SplitType = ESlotSplitType::Default;
	SlotIndex = 0;
	OnSlotRefresh.Clear();
	OnSlotActivated.Clear();
	OnSlotDeactived.Clear();
}

void UAbilityInventorySlotBase::OnInitialize(UAbilityInventoryBase* InInventory, EAbilityItemType InLimitType, ESlotSplitType InSplitType, int32 InSlotIndex)
{
	Inventory = InInventory;
	LimitType = InLimitType;
	SplitType = InSplitType;
	SlotIndex = InSlotIndex;
}

bool UAbilityInventorySlotBase::ContainsItem(FAbilityItem InItem) const
{
	return !IsEmpty() && Item.Match(InItem);
}

bool UAbilityInventorySlotBase::MatchItem(FAbilityItem InItem, bool bPutIn) const
{
	return bPutIn ? (IsEmpty() && MatchItemLimit(InItem, true) || Item.Match(InItem) && GetRemainVolume(InItem) > 0) :
		(!IsEmpty() && MatchItemSplit(InItem) && MatchItemLimit(InItem));
}

bool UAbilityInventorySlotBase::MatchItemSplit(FAbilityItem InItem, bool bForce) const
{
	if(!InItem.IsValid()) return false;

	const auto ItemType = InItem.GetType();
	switch(SplitType)
	{
		case ESlotSplitType::Default:
		case ESlotSplitType::Shortcut:
		case ESlotSplitType::Auxiliary:
		{
			if(!bForce) return ItemType != EAbilityItemType::Skill && ItemType != EAbilityItemType::Equip;
			break;
		}
		case ESlotSplitType::Skill:
		{
			return ItemType == EAbilityItemType::Skill;
		}
		case ESlotSplitType::Equip:
		{
			return ItemType == EAbilityItemType::Equip;
		}
		default: break;
	}
	return false;
}

bool UAbilityInventorySlotBase::MatchItemLimit(FAbilityItem InItem, bool bForce) const
{
	return LimitType == EAbilityItemType::None || InItem.GetType() == LimitType;
}

void UAbilityInventorySlotBase::Reset()
{
	OnItemPreChange(Item, false);
	OnItemChanged(Item, false);
}

void UAbilityInventorySlotBase::Refresh()
{
	if(Item.IsValid() && Item.Count <= 0)
	{
		SetItem(FAbilityItem::Empty, false);
	}
	OnSlotRefresh.Broadcast();
	GetInventory()->OnRefresh.Broadcast();
}

void UAbilityInventorySlotBase::OnItemPreChange(FAbilityItem& InNewItem, bool bBroadcast)
{
	if(bBroadcast)
	{
		if(const auto Agent = Inventory->GetOwnerAgent())
		{
			Agent->OnPreChangeItem(Item);
		}
	}
}

void UAbilityInventorySlotBase::OnItemChanged(FAbilityItem& InOldItem, bool bBroadcast)
{
	if(Item.IsValid())
	{
		const auto Actor = Inventory->GetOwnerAgent<IAbilityActorInterface>();
		if (Actor && Item.GetData().AbilityClass)
		{
			Item.AbilityHandle = Actor->GetAbilitySystemComponent()->K2_GiveAbility(Item.GetData().AbilityClass, Item.Level);
		}
	}
	else
	{
		Item.AbilityHandle = FGameplayAbilitySpecHandle();
	}
	if(bBroadcast)
	{
		if(const auto Agent = Inventory->GetOwnerAgent())
		{
			Agent->OnChangeItem(Item);
			if(IsSelected())
			{
				Agent->OnSelectItem(Item);
			}
		}
	}
}

void UAbilityInventorySlotBase::Replace(UAbilityInventorySlotBase* InSlot)
{
	FAbilityItem _Item = Item;
	FAbilityItem _InItem = InSlot->Item;
	
	InSlot->SetItem(FAbilityItem::Empty);
	SetItem(FAbilityItem::Empty);
	
	InSlot->SetItem(_Item);
	SetItem(_InItem);
}

void UAbilityInventorySlotBase::SetItem(FAbilityItem& InItem, bool bRefresh)
{
	if(Item == InItem) return;
	
	OnItemPreChange(InItem, true);
	
	FAbilityItem OldItem = Item;
	
	Item = InItem;
	Item.Count = FMath::Clamp(Item.Count, 0, GetMaxVolume(InItem));
	Item.Level = FMath::Clamp(Item.Level, 0, GetMaxLevel(InItem));
	Item.InventorySlot = this;
	
	OnItemChanged(OldItem, true);
	
	if(bRefresh)
	{
		Refresh();
	}
}

void UAbilityInventorySlotBase::AddItem(FAbilityItem& InItem)
{
	if (ContainsItem(InItem))
	{
		const int32 Num = InItem.Count - GetRemainVolume(InItem);
		Item.Count = FMath::Clamp(Item.Count + InItem.Count, 0, GetMaxVolume(InItem));
		InItem.Count = FMath::Max(Num, 0);
		Refresh();
	}
	else
	{
		SetItem(InItem);
		const int32 Num = InItem.Count - GetMaxVolume(InItem);
		InItem.Count = FMath::Max(Num, 0);
	}
}

void UAbilityInventorySlotBase::SubItem(FAbilityItem& InItem)
{
	if (ContainsItem(InItem))
	{
		int32 Num = InItem.Count - Item.Count;
		Item.Count = FMath::Clamp(Item.Count - InItem.Count, 0, GetMaxVolume(InItem));
		InItem.Count = FMath::Max(Num, 0);
		Refresh();
	}
}

void UAbilityInventorySlotBase::SplitItem(int32 InCount /*= -1*/)
{
	if (IsEmpty()) return;

	if(InCount == - 1) InCount = Item.Count;
	const int32 Count = Item.Count / InCount;
	for (int32 i = 0; i < InCount - 1; i++)
	{
		FAbilityItem _Item = FAbilityItem(Item, Count);
		auto QueryData = Inventory->QueryItemByRange(EItemQueryType::Split, _Item);
		Inventory->AddItemByQueryData(QueryData, false);
		_Item.Count -= QueryData.Item.Count;
		Item.Count -= _Item.Count;
		if(QueryData.Item.Count > 0) break;
	}
	Refresh();
}

void UAbilityInventorySlotBase::MoveItem(int32 InCount /*= -1*/)
{
	if (IsEmpty()) return;

	if (InCount == -1) InCount = Item.Count;
	FAbilityItem _Item = FAbilityItem(Item, InCount);

	if(Inventory->GetConnectInventory())
	{
		Inventory->GetConnectInventory()->AddItemByRange(_Item);
	}
	else
	{
		switch(GetSplitType())
		{
			case ESlotSplitType::Default:
			{
				Inventory->AddItemBySplitTypes(_Item, {ESlotSplitType::Shortcut, ESlotSplitType::Auxiliary}, false);
				break;
			}
			case ESlotSplitType::Shortcut:
			{
				Inventory->AddItemBySplitTypes(_Item, {ESlotSplitType::Default, ESlotSplitType::Auxiliary}, false);
				break;
			}
			case ESlotSplitType::Auxiliary:
			{
				Inventory->AddItemBySplitTypes(_Item, {ESlotSplitType::Default, ESlotSplitType::Shortcut}, false);
				break;
			}
			case ESlotSplitType::Equip:
			case ESlotSplitType::Skill:
			{
				Inventory->AddItemBySplitTypes(_Item, {ESlotSplitType::Default, ESlotSplitType::Shortcut, ESlotSplitType::Auxiliary}, false);
				break;
			}
			default: break;
		}
	}

	_Item.Count = InCount - _Item.Count;
	SubItem(_Item);
}

void UAbilityInventorySlotBase::UseItem(int32 InCount /*= -1*/)
{
	if (IsEmpty()) return;

	if(InCount == -1) InCount = Item.Count;

	switch (Item.GetType())
	{
		case EAbilityItemType::Prop:
		{
			DON(InCount,
				if(ActiveItem())
				{
					auto _Item = FAbilityItem(Item, 1);
					SubItem(_Item);
				}
				else break;
			)
			break;
		}
		case EAbilityItemType::Equip:
		{
			Inventory->AddItemBySplitType(Item, ESlotSplitType::Equip, false); 
			Refresh();
			break;
		}
		case EAbilityItemType::Skill:
		{
			Inventory->AddItemBySplitType(Item, ESlotSplitType::Skill, false);
			Refresh();
			break;
		}
		default: break;
	}
}

void UAbilityInventorySlotBase::DiscardItem(int32 InCount /*= -1*/, bool bInPlace /*= true*/)
{
	if (IsEmpty()) return;

	if(auto Agent = GetInventory()->GetOwnerAgent())
	{
		FAbilityItem _Item = FAbilityItem(Item, InCount, true);
		Agent->OnDiscardItem(_Item, bInPlace);
		SubItem(_Item);
	}
}

bool UAbilityInventorySlotBase::ActiveItem(bool bPassive /*= false*/)
{
	if(IsEmpty()) return false;

	bool bSuccess = false;
	if(auto Actor = GetInventory()->GetOwnerAgent<IAbilityActorInterface>())
	{
		if(Actor->GetLevelA() >= Item.Level && Actor->GetAbilitySystemComponent()->TryActivateAbility(Item.AbilityHandle))
		{
			OnSlotActivated.Broadcast();
			for(auto Iter : GetInventory()->QueryItemByRange(EItemQueryType::Get, Item).Slots)
			{
				if(Iter != this)
				{
					Iter->OnSlotActivated.Broadcast();
				}
			}
			bSuccess = true;
		}
	}
	if(auto Agent = GetInventory()->GetOwnerAgent())
	{
		Agent->OnActiveItem(Item, bPassive, bSuccess);
	}
    return bSuccess;
}

void UAbilityInventorySlotBase::DeactiveItem(bool bPassive /*= false*/)
{
	if(IsEmpty()) return;
	
	if(auto Actor = GetInventory()->GetOwnerAgent<IAbilityActorInterface>())
	{
		Actor->GetAbilitySystemComponent()->CancelAbilityHandle(Item.AbilityHandle);
		OnSlotDeactived.Broadcast();
	}
	if(auto Agent = GetInventory()->GetOwnerAgent())
	{
		Agent->OnDeactiveItem(Item, bPassive);
	}
}

void UAbilityInventorySlotBase::ClearItem()
{
	if (IsEmpty()) return;

	SetItem(FAbilityItem::Empty);
}

bool UAbilityInventorySlotBase::IsEmpty() const
{
	return Item.IsEmpty();
}

bool UAbilityInventorySlotBase::IsSelected() const
{
	return GetInventory()->GetSelectedSlot(SplitType) == this;
}

bool UAbilityInventorySlotBase::IsMatched() const
{
	if(IsEmpty()) return false;

	return MatchItem(Item);
}

bool UAbilityInventorySlotBase::IsLimitMatched(bool bForce) const
{
	if(IsEmpty()) return false;

	return MatchItemLimit(Item, bForce);
}

bool UAbilityInventorySlotBase::IsSplitMatched(bool bForce) const
{
	if(IsEmpty()) return false;

	return MatchItemSplit(Item, bForce);
}

int32 UAbilityInventorySlotBase::GetRemainVolume(FAbilityItem InItem) const
{
	const FAbilityItem _Item = InItem.IsValid() ? InItem : Item;
	return GetMaxVolume(_Item) - Item.Count;
}

int32 UAbilityInventorySlotBase::GetMaxVolume(FAbilityItem InItem) const
{
	const FAbilityItem _Item = InItem.IsValid() ? InItem : Item;
	return _Item.IsValid() ? _Item.GetData().MaxCount : 0;
}

int32 UAbilityInventorySlotBase::GetMaxLevel(FAbilityItem InItem) const
{
	const FAbilityItem _Item = InItem.IsValid() ? InItem : Item;
	return _Item.IsValid() ? _Item.GetData().MaxLevel : 0;
}

FAbilityInfo UAbilityInventorySlotBase::GetAbilityInfo() const
{
	if(IsEmpty() || !Item.AbilityHandle.IsValid()) return FAbilityInfo();
	
	if(IAbilityActorInterface* AbilityActor = Inventory->GetOwnerAgent<IAbilityActorInterface>())
	{
		if(UAbilitySystemComponentBase* AbilitySystemComp = Cast<UAbilitySystemComponentBase>(AbilityActor->GetAbilitySystemComponent()))
		{
			return AbilitySystemComp->GetAbilityInfoByHandle(Item.AbilityHandle);
		}
	}
	return FAbilityInfo();
}
