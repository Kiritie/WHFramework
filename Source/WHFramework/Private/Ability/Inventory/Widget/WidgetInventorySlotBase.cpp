// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Inventory/Widget/WidgetInventorySlotBase.h"
#include "Ability/Inventory/Slot/InventorySlot.h"
#include "Ability/Inventory/Inventory.h"

UWidgetInventorySlotBase::UWidgetInventorySlotBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	OwnerSlot = nullptr;
}

void UWidgetInventorySlotBase::OnInitialize(UInventorySlot* InOwnerSlot)
{
	if(InOwnerSlot == OwnerSlot) return;
	
	if(OwnerSlot)
	{
		OwnerSlot->OnInventorySlotRefresh.RemoveDynamic(this, &UWidgetInventorySlotBase::OnRefresh);
		OwnerSlot->OnInventorySlotActivated.RemoveDynamic(this, &UWidgetInventorySlotBase::OnActivated);
		OwnerSlot->OnInventorySlotCanceled.RemoveDynamic(this, &UWidgetInventorySlotBase::OnCanceled);
	}

	OwnerSlot = InOwnerSlot;

	if(OwnerSlot)
	{
		OwnerSlot->OnInventorySlotRefresh.AddDynamic(this, &UWidgetInventorySlotBase::OnRefresh);
		OwnerSlot->OnInventorySlotActivated.AddDynamic(this, &UWidgetInventorySlotBase::OnActivated);
		OwnerSlot->OnInventorySlotCanceled.AddDynamic(this, &UWidgetInventorySlotBase::OnCanceled);
	}

	OnRefresh();
}

void UWidgetInventorySlotBase::OnRefresh()
{
	if(!OwnerSlot) return;

	if(!IsEmpty())
	{
		if(IsCooldowning())
		{
			StartCooldown();
		}
		else
		{
			StopCooldown();
		}
	}
	else
	{
		StopCooldown();
	}
}

void UWidgetInventorySlotBase::OnActivated()
{
	if(IsCooldowning())
	{
		StartCooldown();
	}
	else
	{
		StopCooldown();
	}
}

void UWidgetInventorySlotBase::OnCanceled()
{
	StopCooldown();
}

void UWidgetInventorySlotBase::StartCooldown()
{
	GetWorld()->GetTimerManager().SetTimer(CooldownTimerHandle, this, &UWidgetInventorySlotBase::OnCooldown, 0.1f, true);
}

void UWidgetInventorySlotBase::StopCooldown()
{
	GetWorld()->GetTimerManager().ClearTimer(CooldownTimerHandle);
}

void UWidgetInventorySlotBase::OnCooldown()
{
	if(OwnerSlot)
	{
		const FAbilityInfo CooldownInfo = OwnerSlot->GetAbilityInfo();
		if(CooldownInfo.CooldownRemaining <= 0.f)
		{
			StopCooldown();
		}
	}
}

void UWidgetInventorySlotBase::SplitItem(int InCount)
{
	if(OwnerSlot)
	{
		OwnerSlot->SplitItem(InCount);
	}
}

void UWidgetInventorySlotBase::MoveItem(int InCount)
{
	if(OwnerSlot)
	{
		OwnerSlot->MoveItem(InCount);
	}
}

void UWidgetInventorySlotBase::UseItem(int InCount)
{
	if(OwnerSlot)
	{
		OwnerSlot->UseItem(InCount);
	}
}

void UWidgetInventorySlotBase::DiscardItem(int InCount)
{
	if(OwnerSlot)
	{
		OwnerSlot->DiscardItem(InCount, false);
	}
}

bool UWidgetInventorySlotBase::IsEmpty() const
{
	if (OwnerSlot) return OwnerSlot->IsEmpty();
	return true;
}

bool UWidgetInventorySlotBase::IsCooldowning() const
{
	if(OwnerSlot) return OwnerSlot->IsMatched() && OwnerSlot->GetAbilityInfo().IsCooldownning();
	return false;
}

FAbilityItem& UWidgetInventorySlotBase::GetItem() const
{
	if(OwnerSlot) return OwnerSlot->GetItem();
	return FAbilityItem::Empty;
}

UInventory* UWidgetInventorySlotBase::GetInventory() const
{
	if(OwnerSlot) return OwnerSlot->GetInventory();
	return nullptr;
}
