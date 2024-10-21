// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Ability/Inventory/Slot/AbilityInventorySlotBase.h"

#include "Ability/AbilityModuleStatics.h"
#include "Ability/Abilities/ItemAbilityBase.h"
#include "Ability/Components/AbilitySystemComponentBase.h"
#include "Ability/Item/AbilityItemDataBase.h"
#include "Ability/Inventory/AbilityInventoryBase.h"
#include "Ability/Inventory/AbilityInventoryAgentInterface.h"
#include "Ability/Vitality/AbilityVitalityInterface.h"
#include "Voxel/VoxelModule.h"

UAbilityInventorySlotBase::UAbilityInventorySlotBase()
{
	Item = FAbilityItem::Empty;
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

bool UAbilityInventorySlotBase::CheckSlot(FAbilityItem& InItem) const
{
	return LimitType == EAbilityItemType::None || InItem.GetType() == LimitType;
}

bool UAbilityInventorySlotBase::CanPutIn(FAbilityItem& InItem) const
{
	return (IsEmpty() && CheckSlot(InItem)) || (Item.EqualType(InItem) && GetRemainVolume(InItem) > 0);
}

bool UAbilityInventorySlotBase::IsMatch(FAbilityItem InItem, bool bForce) const
{
	if(!InItem.IsValid()) return false;

	const auto ItemType = InItem.GetType();
	switch(SplitType)
	{
		case ESlotSplitType::Default:
		case ESlotSplitType::Shortcut:
		case ESlotSplitType::Auxiliary:
		{
			if(!bForce)
			{
				return ItemType != EAbilityItemType::Skill && ItemType != EAbilityItemType::Equip;
			}
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

bool UAbilityInventorySlotBase::Contains(FAbilityItem& InItem) const
{
	return !IsEmpty() && Item.EqualType(InItem);
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

void UAbilityInventorySlotBase::OnItemPreChange(FAbilityItem& InNewItem)
{
	
}

void UAbilityInventorySlotBase::OnItemChanged(FAbilityItem& InOldItem)
{
	if(Item.IsValid())
	{
		const auto Vitality = Inventory->GetOwnerAgent<IAbilityVitalityInterface>();
		if (Vitality && Item.GetData().AbilityClass)
		{
			Item.AbilityHandle = Vitality->GetAbilitySystemComponent()->K2_GiveAbility(Item.GetData().AbilityClass, Item.Level);
		}
	}
	else
	{
		Item.AbilityHandle = FGameplayAbilitySpecHandle();
	}
	if(IsSelected())
	{
		if(const auto Agent = Inventory->GetOwnerAgent())
		{
			Agent->OnSelectItem(SplitType, GetItem());
		}
	}
}

void UAbilityInventorySlotBase::Replace(UAbilityInventorySlotBase* InSlot)
{
	auto tmpItem = GetItem();
	SetItem(InSlot->GetItem());
	InSlot->SetItem(tmpItem);
}

void UAbilityInventorySlotBase::SetItem(FAbilityItem& InItem, bool bRefresh)
{
	OnItemPreChange(InItem);
	FAbilityItem OldItem = Item;
	Item = InItem;
	Item.Count = FMath::Clamp(Item.Count, 0, GetMaxVolume(InItem));
	OnItemChanged(OldItem);
	if(bRefresh)
	{
		Refresh();
	}
}

void UAbilityInventorySlotBase::AddItem(FAbilityItem& InItem)
{
	if (Contains(InItem))
	{
		const int32 tmpNum = InItem.Count - GetRemainVolume(InItem);
		Item.Count = FMath::Clamp(Item.Count + InItem.Count, 0, GetMaxVolume(InItem));
		InItem.Count = FMath::Max(tmpNum, 0);
		Refresh();
	}
	else
	{
		SetItem(InItem);
		const int32 tmpNum = InItem.Count - GetMaxVolume(InItem);
		InItem.Count = FMath::Max(tmpNum, 0);
	}
}

void UAbilityInventorySlotBase::SubItem(FAbilityItem& InItem)
{
	if (Contains(InItem))
	{
		int32 tmpNum = InItem.Count - Item.Count;
		Item.Count = FMath::Clamp(Item.Count - InItem.Count, 0, GetMaxVolume(InItem));
		InItem.Count = FMath::Max(tmpNum, 0);
		Refresh();
	}
}

void UAbilityInventorySlotBase::SplitItem(int32 InCount /*= -1*/)
{
	if (IsEmpty()) return;

	if(InCount == - 1) InCount = Item.Count;
	const int32 tmpCount = Item.Count / InCount;
	auto ItemQueryInfo = Inventory->QueryItemByRange(EItemQueryType::Add, Item);
	for (int32 i = 0; i < InCount; i++)
	{
		FAbilityItem tmpItem = FAbilityItem(Item, tmpCount);
		Item.Count -= tmpItem.Count;
		for (int32 j = 0; j < ItemQueryInfo.Slots.Num(); j++)
		{
			if (ItemQueryInfo.Slots[j]->IsEmpty() && ItemQueryInfo.Slots[j] != this)
			{
				ItemQueryInfo.Slots[j]->SetItem(tmpItem);
				ItemQueryInfo.Slots.RemoveAt(j);
				break;
			}
		}
	}
	Refresh();
}

void UAbilityInventorySlotBase::MoveItem(int32 InCount /*= -1*/)
{
	if (IsEmpty()) return;

	if (InCount == -1) InCount = Item.Count;
	FAbilityItem tmpItem = FAbilityItem(Item, InCount);

	if(Inventory->GetConnectInventory())
	{
		Inventory->GetConnectInventory()->AddItemByRange(tmpItem);
	}
	else
	{
		switch(GetSplitType())
		{
			case ESlotSplitType::Default:
			{
				Inventory->AddItemBySplitTypes(tmpItem, {ESlotSplitType::Shortcut, ESlotSplitType::Auxiliary}, false);
				break;
			}
			case ESlotSplitType::Shortcut:
			{
				Inventory->AddItemBySplitTypes(tmpItem, {ESlotSplitType::Default, ESlotSplitType::Auxiliary}, false);
				break;
			}
			case ESlotSplitType::Auxiliary:
			{
				Inventory->AddItemBySplitTypes(tmpItem, {ESlotSplitType::Default, ESlotSplitType::Shortcut}, false);
				break;
			}
			case ESlotSplitType::Equip:
			case ESlotSplitType::Skill:
			{
				Inventory->AddItemBySplitTypes(tmpItem, {ESlotSplitType::Default, ESlotSplitType::Shortcut, ESlotSplitType::Auxiliary}, false);
				break;
			}
			default: break;
		}
	}

	tmpItem.Count = InCount - tmpItem.Count;
	SubItem(tmpItem);
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
					auto tmpItem = FAbilityItem(Item, 1);
					SubItem(tmpItem);
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
		FAbilityItem tmpItem = FAbilityItem(Item, InCount);
		Agent->OnDiscardItem(tmpItem, bInPlace);
		SubItem(tmpItem);
	}
}

bool UAbilityInventorySlotBase::ActiveItem(bool bPassive /*= false*/)
{
	if(IsEmpty()) return false;

	bool bSuccess = false;
	if(auto Vitality = GetInventory()->GetOwnerAgent<IAbilityVitalityInterface>())
	{
		if(Vitality->GetAbilitySystemComponent()->TryActivateAbility(Item.AbilityHandle))
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

void UAbilityInventorySlotBase::CancelItem(bool bPassive /*= false*/)
{
	if(IsEmpty()) return;
	
	if(auto Vitality = GetInventory()->GetOwnerAgent<IAbilityVitalityInterface>())
	{
		Vitality->GetAbilitySystemComponent()->CancelAbilityHandle(Item.AbilityHandle);
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

bool UAbilityInventorySlotBase::IsMatched(bool bForce) const
{
	if(IsEmpty()) return false;

	return IsMatch(Item, bForce);
}

int32 UAbilityInventorySlotBase::GetRemainVolume(FAbilityItem InItem) const
{
	const FAbilityItem TmpItem = InItem.IsValid() ? InItem : Item;
	return GetMaxVolume(TmpItem) - Item.Count;
}

int32 UAbilityInventorySlotBase::GetMaxVolume(FAbilityItem InItem) const
{
	const FAbilityItem TmpItem = InItem.IsValid() ? InItem : Item;
	return TmpItem.IsValid() ? TmpItem.GetData().MaxCount : 0;
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