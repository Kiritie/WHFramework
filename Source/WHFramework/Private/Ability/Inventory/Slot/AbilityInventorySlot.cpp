// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Ability/Inventory/Slot/AbilityInventorySlot.h"

#include "Ability/AbilityModuleStatics.h"
#include "Ability/Abilities/ItemAbilityBase.h"
#include "Ability/Components/AbilitySystemComponentBase.h"
#include "Ability/Item/AbilityItemDataBase.h"
#include "Ability/Inventory/AbilityInventoryBase.h"
#include "Ability/Inventory/AbilityInventoryAgentInterface.h"
#include "Ability/Vitality/AbilityVitalityInterface.h"
#include "Voxel/VoxelModule.h"

UAbilityInventorySlot::UAbilityInventorySlot()
{
	Item = FAbilityItem::Empty;
	Inventory = nullptr;
	LimitType = EAbilityItemType::None;
	SplitType = ESlotSplitType::Default;
}

void UAbilityInventorySlot::OnInitialize(UAbilityInventoryBase* InInventory, EAbilityItemType InLimitType, ESlotSplitType InSplitType, int32 InSlotIndex)
{
	Inventory = InInventory;
	LimitType = InLimitType;
	SplitType = InSplitType;
}

void UAbilityInventorySlot::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	
}

void UAbilityInventorySlot::OnDespawn_Implementation(bool bRecovery)
{
	SetItem(FAbilityItem::Empty);
	Inventory = nullptr;
	LimitType = EAbilityItemType::None;
	SplitType = ESlotSplitType::Default;
}

bool UAbilityInventorySlot::CheckSlot(FAbilityItem& InItem) const
{
	return LimitType == EAbilityItemType::None || InItem.GetType() == LimitType;
}

bool UAbilityInventorySlot::CanPutIn(FAbilityItem& InItem) const
{
	return (IsEmpty() && CheckSlot(InItem)) || (Item.EqualType(InItem) && GetRemainVolume(InItem) > 0);
}

bool UAbilityInventorySlot::IsMatch(FAbilityItem InItem, bool bForce) const
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

bool UAbilityInventorySlot::Contains(FAbilityItem& InItem) const
{
	return !IsEmpty() && Item.EqualType(InItem);
}

void UAbilityInventorySlot::Refresh()
{
	if(Item.IsValid() && Item.Count <= 0)
	{
		SetItem(FAbilityItem::Empty, false);
	}
	OnInventorySlotRefresh.Broadcast();
	GetInventory()->OnRefresh.Broadcast();
}

void UAbilityInventorySlot::OnItemPreChange(FAbilityItem& InNewItem)
{
	
}

void UAbilityInventorySlot::OnItemChanged(FAbilityItem& InOldItem)
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
		if(const auto InventoryAgent = Inventory->GetOwnerAgent())
		{
			InventoryAgent->OnSelectItem(GetItem());
		}
	}
}

void UAbilityInventorySlot::Replace(UAbilityInventorySlot* InSlot)
{
	auto tmpItem = GetItem();
	SetItem(InSlot->GetItem());
	InSlot->SetItem(tmpItem);
}

void UAbilityInventorySlot::SetItem(FAbilityItem& InItem, bool bRefresh)
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

void UAbilityInventorySlot::AddItem(FAbilityItem& InItem)
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
		InItem.Count -= GetMaxVolume(InItem);
	}
}

void UAbilityInventorySlot::SubItem(FAbilityItem& InItem)
{
	if (Contains(InItem))
	{
		int32 tmpNum = InItem.Count - Item.Count;
		Item.Count = FMath::Clamp(Item.Count - InItem.Count, 0, GetMaxVolume(InItem));
		InItem.Count = FMath::Max(tmpNum, 0);
		Refresh();
	}
}

void UAbilityInventorySlot::SplitItem(int32 InCount /*= -1*/)
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

void UAbilityInventorySlot::MoveItem(int32 InCount /*= -1*/)
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
				Inventory->AddItemBySplitTypes(tmpItem, {ESlotSplitType::Shortcut, ESlotSplitType::Auxiliary});
				break;
			}
			case ESlotSplitType::Shortcut:
			{
				Inventory->AddItemBySplitTypes(tmpItem, {ESlotSplitType::Default, ESlotSplitType::Auxiliary});
				break;
			}
			case ESlotSplitType::Auxiliary:
			{
				Inventory->AddItemBySplitTypes(tmpItem, {ESlotSplitType::Default, ESlotSplitType::Shortcut});
				break;
			}
			case ESlotSplitType::Equip:
			case ESlotSplitType::Skill:
			{
				Inventory->AddItemBySplitTypes(tmpItem, {ESlotSplitType::Default, ESlotSplitType::Shortcut, ESlotSplitType::Auxiliary});
				break;
			}
			default: break;
		}
	}

	tmpItem.Count = InCount - tmpItem.Count;
	SubItem(tmpItem);
}

void UAbilityInventorySlot::UseItem(int32 InCount /*= -1*/)
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
			Inventory->AddItemBySplitType(Item, ESlotSplitType::Equip); 
			Refresh();
			break;
		}
		case EAbilityItemType::Skill:
		{
			Inventory->AddItemBySplitType(Item, ESlotSplitType::Skill);
			Refresh();
			break;
		}
		default: break;
	}
}

void UAbilityInventorySlot::DiscardItem(int32 InCount /*= -1*/, bool bInPlace /*= true*/)
{
	if (IsEmpty()) return;

	if(auto Agent = GetInventory()->GetOwnerAgent())
	{
		FAbilityItem tmpItem = FAbilityItem(Item, InCount);
		Agent->OnDiscardItem(tmpItem, bInPlace);
		SubItem(tmpItem);
	}
}

bool UAbilityInventorySlot::ActiveItem(bool bPassive /*= false*/)
{
	if(IsEmpty()) return false;

	bool bSuccess = false;
	if(auto Vitality = GetInventory()->GetOwnerAgent<IAbilityVitalityInterface>())
	{
		if(Vitality->GetAbilitySystemComponent()->TryActivateAbility(Item.AbilityHandle))
		{
			OnInventorySlotActivated.Broadcast();
			for(auto Iter : GetInventory()->QueryItemByRange(EItemQueryType::Get, Item).Slots)
			{
				if(Iter != this)
				{
					Iter->OnInventorySlotActivated.Broadcast();
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

void UAbilityInventorySlot::CancelItem(bool bPassive /*= false*/)
{
	if(IsEmpty()) return;
	
	if(auto Vitality = GetInventory()->GetOwnerAgent<IAbilityVitalityInterface>())
	{
		Vitality->GetAbilitySystemComponent()->CancelAbilityHandle(Item.AbilityHandle);
		OnInventorySlotCanceled.Broadcast();
	}
	if(auto Agent = GetInventory()->GetOwnerAgent())
	{
		Agent->OnCancelItem(Item, bPassive);
	}
}

void UAbilityInventorySlot::ClearItem()
{
	if (IsEmpty()) return;

	SetItem(FAbilityItem::Empty);
}

bool UAbilityInventorySlot::IsEmpty() const
{
	return Item.IsEmpty();
}

bool UAbilityInventorySlot::IsSelected() const
{
	return GetInventory()->GetSelectedSlot() == this;
}

bool UAbilityInventorySlot::IsMatched(bool bForce) const
{
	if(IsEmpty()) return false;

	return IsMatch(Item, bForce);
}

int32 UAbilityInventorySlot::GetSplitIndex(ESlotSplitType InSplitType)
{
	return GetInventory()->GetSlotsBySplitType(InSplitType).Find(this);
}

int32 UAbilityInventorySlot::GetRemainVolume(FAbilityItem InItem) const
{
	const FAbilityItem TmpItem = InItem.IsValid() ? InItem : Item;
	return GetMaxVolume(TmpItem) - Item.Count;
}

int32 UAbilityInventorySlot::GetMaxVolume(FAbilityItem InItem) const
{
	const FAbilityItem TmpItem = InItem.IsValid() ? InItem : Item;
	return TmpItem.IsValid() ? TmpItem.GetData().MaxCount : 0;
}

FAbilityInfo UAbilityInventorySlot::GetAbilityInfo() const
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
