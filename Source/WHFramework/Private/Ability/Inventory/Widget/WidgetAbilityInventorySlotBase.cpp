// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Inventory/Widget/WidgetAbilityInventorySlotBase.h"
#include "Ability/Inventory/Slot/AbilityInventorySlotBase.h"
#include "Ability/Inventory/AbilityInventoryBase.h"
#include "Ability/Item/Widget/WidgetAbilityItemBase.h"
#include "Ability/Item/Widget/WidgetAbilityDragItemBase.h"
#include "Asset/AssetModuleStatics.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "Widget/Screen/UserWidgetBase.h"

UWidgetAbilityInventorySlotBase::UWidgetAbilityInventorySlotBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ClickMethod = EButtonClickMethod::MouseDown;

	MatchStyle = nullptr;
	MismatchStyle = nullptr;
	DefaultStyle = nullptr;
	
	OwnerSlot = nullptr;
	CooldownTimerHandle = FTimerHandle();
}

void UWidgetAbilityInventorySlotBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);
}

void UWidgetAbilityInventorySlotBase::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);

	SetStyle(DefaultStyle);

	OwnerSlot = nullptr;
	StopCooldown();
}

void UWidgetAbilityInventorySlotBase::OnCreate(UUserWidgetBase* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnCreate(InOwner, InParams);

	DefaultStyle = Style;
}

void UWidgetAbilityInventorySlotBase::OnInitialize(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		const auto InOwnerSlot = InParams[0].GetObjectValue<UAbilityInventorySlotBase>();
		if(InOwnerSlot == OwnerSlot) return;
	
		if(OwnerSlot)
		{
			OwnerSlot->OnSlotRefresh.RemoveDynamic(this, &UWidgetAbilityInventorySlotBase::OnRefresh);
			OwnerSlot->OnSlotActivated.RemoveDynamic(this, &UWidgetAbilityInventorySlotBase::OnActivated);
			OwnerSlot->OnSlotDeactived.RemoveDynamic(this, &UWidgetAbilityInventorySlotBase::OnDeactived);
		}

		OwnerSlot = InOwnerSlot;

		if(OwnerSlot)
		{
			OwnerSlot->OnSlotRefresh.AddDynamic(this, &UWidgetAbilityInventorySlotBase::OnRefresh);
			OwnerSlot->OnSlotActivated.AddDynamic(this, &UWidgetAbilityInventorySlotBase::OnActivated);
			OwnerSlot->OnSlotDeactived.AddDynamic(this, &UWidgetAbilityInventorySlotBase::OnDeactived);
		}
	}
	Super::OnInitialize(InParams);
}

void UWidgetAbilityInventorySlotBase::OnRefresh()
{
	Super::OnRefresh();
	
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

void UWidgetAbilityInventorySlotBase::OnDestroy(bool bRecovery)
{
	Super::OnDestroy(bRecovery);
}

bool UWidgetAbilityInventorySlotBase::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);

	const auto PayloadSlot = Cast<UWidgetAbilityInventorySlotBase>(InOperation->Payload);
	if (PayloadSlot && PayloadSlot != this && !PayloadSlot->IsEmpty())
	{
		SetStyle(DefaultStyle);

		FAbilityItem& _Item = PayloadSlot->GetItem();
		if(OwnerSlot->MatchItemLimit(_Item, true))
		{
			if (OwnerSlot->ContainsItem(_Item))
			{
				OwnerSlot->AddItem(_Item);
				PayloadSlot->OwnerSlot->Refresh();
			}
			else if(OwnerSlot->IsEmpty() || PayloadSlot->OwnerSlot->MatchItemLimit(OwnerSlot->GetItem(), true))
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

	const auto PayloadSlot = Cast<UWidgetAbilityInventorySlotBase>(InOperation->Payload);
	if (PayloadSlot && !PayloadSlot->IsEmpty())
	{
		TSubclassOf<UCommonButtonStyle> _Style;
		FAbilityItem& _Item = PayloadSlot->GetItem();
		if(OwnerSlot->MatchItemLimit(_Item, true))
		{
			if (OwnerSlot->ContainsItem(_Item))
			{
				_Style = MatchStyle;
			}
			else if(OwnerSlot->IsEmpty() || PayloadSlot->OwnerSlot->MatchItemLimit(OwnerSlot->GetItem(), true))
			{
				_Style = MatchStyle;
			}
			else
			{
				_Style = MismatchStyle;
			}
		}
		else
		{
			_Style = MismatchStyle;
		}
		SetStyle(_Style);
	}
}

void UWidgetAbilityInventorySlotBase::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);

	const auto PayloadSlot = Cast<UWidgetAbilityInventorySlotBase>(InOperation->Payload);
	if (PayloadSlot && !PayloadSlot->IsEmpty())
	{
		SetStyle(DefaultStyle);
	}
}

void UWidgetAbilityInventorySlotBase::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

	if(!IsEmpty())
	{
		OutOperation = UWidgetBlueprintLibrary::CreateDragDropOperation(UDragDropOperation::StaticClass());
		OutOperation->Payload = this;
		OutOperation->DefaultDragVisual = UObjectPoolModuleStatics::SpawnObject<UWidgetAbilityDragItemBase>(nullptr, { &GetItem() }, false, UAssetModuleStatics::GetStaticClass(FName("DragItem")));
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

void UWidgetAbilityInventorySlotBase::OnDeactived_Implementation()
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
	if(OwnerSlot) return OwnerSlot->IsSplitMatched() && OwnerSlot->GetAbilityInfo().IsCooldownning();
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
