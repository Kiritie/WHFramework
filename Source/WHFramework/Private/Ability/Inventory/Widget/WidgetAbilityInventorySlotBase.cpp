// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Inventory/Widget/WidgetAbilityInventorySlotBase.h"
#include "Ability/Inventory/Slot/AbilityInventorySlot.h"
#include "Ability/Inventory/AbilityInventoryBase.h"
#include "Ability/Inventory/Widget/WidgetAbilityInventoryItemBase.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Widget/Screen/UMG/UserWidgetBase.h"

UWidgetAbilityInventorySlotBase::UWidgetAbilityInventorySlotBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	OwnerSlot = nullptr;
	DragVisualClass = nullptr;
	CooldownTimerHandle = FTimerHandle();
}

void UWidgetAbilityInventorySlotBase::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InParams);
}

void UWidgetAbilityInventorySlotBase::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);
	
	CooldownTimerHandle = FTimerHandle();
}

void UWidgetAbilityInventorySlotBase::OnCreate_Implementation(UUserWidgetBase* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnCreate_Implementation(InOwner, InParams);
}

void UWidgetAbilityInventorySlotBase::OnInitialize_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		const auto InOwnerSlot = InParams[0].GetObjectValue<UAbilityInventorySlot>();
		if(InOwnerSlot == OwnerSlot) return;
	
		if(OwnerSlot)
		{
			OwnerSlot->OnInventorySlotRefresh.RemoveDynamic(this, &UWidgetAbilityInventorySlotBase::OnRefresh);
			OwnerSlot->OnInventorySlotActivated.RemoveDynamic(this, &UWidgetAbilityInventorySlotBase::OnActivated);
			OwnerSlot->OnInventorySlotCanceled.RemoveDynamic(this, &UWidgetAbilityInventorySlotBase::OnCanceled);
		}

		OwnerSlot = InOwnerSlot;

		if(OwnerSlot)
		{
			OwnerSlot->OnInventorySlotRefresh.AddDynamic(this, &UWidgetAbilityInventorySlotBase::OnRefresh);
			OwnerSlot->OnInventorySlotActivated.AddDynamic(this, &UWidgetAbilityInventorySlotBase::OnActivated);
			OwnerSlot->OnInventorySlotCanceled.AddDynamic(this, &UWidgetAbilityInventorySlotBase::OnCanceled);
		}
	}
	Super::OnInitialize_Implementation(InParams);
}

void UWidgetAbilityInventorySlotBase::OnRefresh_Implementation()
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

void UWidgetAbilityInventorySlotBase::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();
}

bool UWidgetAbilityInventorySlotBase::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);

	const auto PayloadSlot = Cast<UWidgetAbilityInventorySlotBase>(InOperation->Payload);
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

void UWidgetAbilityInventorySlotBase::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);
}

void UWidgetAbilityInventorySlotBase::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
}

void UWidgetAbilityInventorySlotBase::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

	if(!IsEmpty() && DragVisualClass)
	{
		OutOperation = UWidgetBlueprintLibrary::CreateDragDropOperation(UDragDropOperation::StaticClass());
		OutOperation->Payload = this;
		OutOperation->DefaultDragVisual = Owner->CreateSubWidget<UWidgetAbilityInventoryItemBase>({ &GetItem() }, DragVisualClass);
	}
}

void UWidgetAbilityInventorySlotBase::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
}

void UWidgetAbilityInventorySlotBase::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
}

FReply UWidgetAbilityInventorySlotBase::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UWidgetAbilityInventorySlotBase::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, FKey("LeftMouseButton")).NativeReply;
}

void UWidgetAbilityInventorySlotBase::OnActivated_Implementation()
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

void UWidgetAbilityInventorySlotBase::OnCanceled_Implementation()
{
	StopCooldown();
}

void UWidgetAbilityInventorySlotBase::StartCooldown_Implementation()
{
	GetWorld()->GetTimerManager().SetTimer(CooldownTimerHandle, this, &UWidgetAbilityInventorySlotBase::OnCooldown, 0.1f, true);
}

void UWidgetAbilityInventorySlotBase::StopCooldown_Implementation()
{
	GetWorld()->GetTimerManager().ClearTimer(CooldownTimerHandle);
}

void UWidgetAbilityInventorySlotBase::OnCooldown_Implementation()
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

void UWidgetAbilityInventorySlotBase::SplitItem_Implementation(int InCount)
{
	if(OwnerSlot)
	{
		OwnerSlot->SplitItem(InCount);
	}
}

void UWidgetAbilityInventorySlotBase::MoveItem_Implementation(int InCount)
{
	if(OwnerSlot)
	{
		OwnerSlot->MoveItem(InCount);
	}
}

void UWidgetAbilityInventorySlotBase::UseItem_Implementation(int InCount)
{
	if(OwnerSlot)
	{
		OwnerSlot->UseItem(InCount);
	}
}

void UWidgetAbilityInventorySlotBase::DiscardItem_Implementation(int InCount)
{
	if(OwnerSlot)
	{
		OwnerSlot->DiscardItem(InCount, false);
	}
}

bool UWidgetAbilityInventorySlotBase::IsEmpty() const
{
	if (OwnerSlot) return OwnerSlot->IsEmpty();
	return true;
}

bool UWidgetAbilityInventorySlotBase::IsCooldowning() const
{
	if(OwnerSlot) return OwnerSlot->IsMatched() && OwnerSlot->GetAbilityInfo().IsCooldownning();
	return false;
}

FAbilityItem& UWidgetAbilityInventorySlotBase::GetItem() const
{
	if(OwnerSlot) return OwnerSlot->GetItem();
	return FAbilityItem::Empty;
}

UAbilityInventoryBase* UWidgetAbilityInventorySlotBase::GetInventory() const
{
	if(OwnerSlot) return OwnerSlot->GetInventory();
	return nullptr;
}
