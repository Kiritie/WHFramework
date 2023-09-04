// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Inventory/Widget/WidgetInventorySlotBase.h"
#include "Ability/Inventory/Slot/InventorySlot.h"
#include "Ability/Inventory/Inventory.h"

UWidgetInventorySlotBase::UWidgetInventorySlotBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	OwnerSlot = nullptr;
	CooldownTimerHandle = FTimerHandle();
}

void UWidgetInventorySlotBase::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{

}

void UWidgetInventorySlotBase::OnDespawn_Implementation(bool bRecovery)
{
	OwnerSlot = nullptr;
	CooldownTimerHandle = FTimerHandle();
	RemoveFromParent();
}

void UWidgetInventorySlotBase::OnCreate_Implementation(UUserWidgetBase* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnCreate_Implementation(InOwner, InParams);
}

void UWidgetInventorySlotBase::OnInitialize_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		const auto InOwnerSlot = InParams[0].GetObjectValue<UInventorySlot>();
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
	}
	Super::OnInitialize_Implementation(InParams);
}

void UWidgetInventorySlotBase::OnRefresh_Implementation()
{
	Super::OnRefresh_Implementation();
	
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

void UWidgetInventorySlotBase::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();
}

void UWidgetInventorySlotBase::OnActivated_Implementation()
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

void UWidgetInventorySlotBase::OnCanceled_Implementation()
{
	StopCooldown();
}

void UWidgetInventorySlotBase::StartCooldown_Implementation()
{
	GetWorld()->GetTimerManager().SetTimer(CooldownTimerHandle, this, &UWidgetInventorySlotBase::OnCooldown, 0.1f, true);
}

void UWidgetInventorySlotBase::StopCooldown_Implementation()
{
	GetWorld()->GetTimerManager().ClearTimer(CooldownTimerHandle);
}

void UWidgetInventorySlotBase::OnCooldown_Implementation()
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

void UWidgetInventorySlotBase::SplitItem_Implementation(int InCount)
{
	if(OwnerSlot)
	{
		OwnerSlot->SplitItem(InCount);
	}
}

void UWidgetInventorySlotBase::MoveItem_Implementation(int InCount)
{
	if(OwnerSlot)
	{
		OwnerSlot->MoveItem(InCount);
	}
}

void UWidgetInventorySlotBase::UseItem_Implementation(int InCount)
{
	if(OwnerSlot)
	{
		OwnerSlot->UseItem(InCount);
	}
}

void UWidgetInventorySlotBase::DiscardItem_Implementation(int InCount)
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
