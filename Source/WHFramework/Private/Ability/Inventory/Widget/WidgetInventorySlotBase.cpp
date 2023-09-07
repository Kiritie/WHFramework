// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Inventory/Widget/WidgetInventorySlotBase.h"
#include "Ability/Inventory/Slot/InventorySlot.h"
#include "Ability/Inventory/Inventory.h"
#include "Ability/Inventory/Widget/WidgetInventoryItemBase.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Widget/Screen/UMG/UserWidgetBase.h"

UWidgetInventorySlotBase::UWidgetInventorySlotBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	OwnerSlot = nullptr;
	DragVisualClass = nullptr;
	CooldownTimerHandle = FTimerHandle();
}

void UWidgetInventorySlotBase::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InParams);
}

void UWidgetInventorySlotBase::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);
	
	CooldownTimerHandle = FTimerHandle();
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

bool UWidgetInventorySlotBase::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);

	const auto PayloadSlot = Cast<UWidgetInventorySlotBase>(InOperation->Payload);
	if (PayloadSlot && PayloadSlot != this && !PayloadSlot->IsEmpty())
	{
		FAbilityItem& tmpItem = PayloadSlot->GetItem();
		if(OwnerSlot->CheckSlot(tmpItem))
		{
			if (OwnerSlot->Contains(tmpItem))
			{
				OwnerSlot->AddItem(tmpItem);
				PayloadSlot->OwnerSlot->Refresh();
			}
			else
			{
				OwnerSlot->Replace(PayloadSlot->OwnerSlot);
			}
			return true;
		}
	}
	return false;
}

void UWidgetInventorySlotBase::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);
}

void UWidgetInventorySlotBase::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
}

void UWidgetInventorySlotBase::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

	if(!IsEmpty() && DragVisualClass)
	{
		OutOperation = UWidgetBlueprintLibrary::CreateDragDropOperation(UDragDropOperation::StaticClass());
		OutOperation->Payload = this;
		OutOperation->DefaultDragVisual = Owner->CreateSubWidget<UWidgetInventoryItemBase>({ &GetItem() }, DragVisualClass);
	}
}

void UWidgetInventorySlotBase::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
}

void UWidgetInventorySlotBase::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
}

FReply UWidgetInventorySlotBase::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UWidgetInventorySlotBase::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if(InMouseEvent.GetEffectingButton() == FKey("RightMouseButton"))
	{
		if(InMouseEvent.IsLeftControlDown())
		{
			MoveItem(-1);
		}
		else if(InMouseEvent.IsLeftShiftDown())
		{
			UseItem(-1);
		}
		else
		{
			UseItem(1);
		}
		return FReply::Handled();
	}
	else if(InMouseEvent.GetEffectingButton() == FKey("MiddleMouseButton"))
	{
		if(InMouseEvent.IsLeftShiftDown())
		{
			DiscardItem(-1);
		}
		else
		{
			DiscardItem(1);
		}
		return FReply::Handled();
	}
	return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, FKey("LeftMouseButton")).NativeReply;
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
