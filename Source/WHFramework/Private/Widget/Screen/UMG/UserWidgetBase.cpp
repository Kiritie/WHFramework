// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/Screen/UMG/UserWidgetBase.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Event/EventModuleStatics.h"
#include "Event/Handle/Widget/EventHandle_UserWidgetClosed.h"
#include "Event/Handle/Widget/EventHandle_UserWidgetOpened.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "Widget/WidgetModule.h"
#include "Widget/WidgetModuleStatics.h"
#include "Input/InputModuleStatics.h"
#include "Widget/Animator/WidgetAnimatorBase.h"
#include "Widget/Screen/UMG/SubWidgetBase.h"

UUserWidgetBase::UUserWidgetBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bWidgetTickAble = false;
	WidgetType = EWidgetType::Permanent;
	WidgetName = NAME_None;
	ParentName = NAME_None;
	ParentSlot = NAME_None;
	WidgetZOrder = 0;
	WidgetAnchors = FAnchors(0.f, 0.f, 0.f, 0.f);
	bWidgetAutoSize = false;
	WidgetDrawSize = FVector2D(0.f);
	WidgetOffsets = FMargin(0.f);
	WidgetAlignment = FVector2D(0.f);
	WidgetCreateType = EWidgetCreateType::None;
	WidgetOpenType = EWidgetOpenType::SelfHitTestInvisible;
	WidgetOpenFinishType = EWidgetOpenFinishType::Instant;
	WidgetOpenFinishTime = 0.f;
	WidgetOpenAnimator = nullptr;
	WidgetCloseType = EWidgetCloseType::Hidden;
	WidgetCloseFinishType = EWidgetCloseFinishType::Instant;
	WidgetCloseFinishTime = 0.f;
	WidgetCloseAnimator = nullptr;
	WidgetRefreshType = EWidgetRefreshType::Procedure;
	WidgetRefreshTime = 0.f;
	WidgetState = EScreenWidgetState::None;
	WidgetParams = TArray<FParameter>();
	WidgetInputMode = EInputMode::None;
	OwnerObject = nullptr;
	LastTemporary = nullptr;
	ParentWidget = nullptr;
	TemporaryChild = nullptr;
	ChildWidgets = TArray<IScreenWidgetInterface*>();
}

FReply UUserWidgetBase::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UUserWidgetBase::NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	return Super::NativeOnKeyUp(InGeometry, InKeyEvent);
}

void UUserWidgetBase::OnTick_Implementation(float DeltaSeconds)
{
	
}

void UUserWidgetBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	
}

void UUserWidgetBase::OnDespawn_Implementation(bool bRecovery)
{
	
}

void UUserWidgetBase::OnCreate(UObject* InOwner, const TArray<FParameter>& InParams)
{
	WidgetParams = InParams;

	if(ParentWidget)
	{
		ParentWidget->RemoveChild(this);
	}
	if(const auto InParent = Cast<UUserWidgetBase>(InOwner))
	{
		ParentWidget = InParent;
		ParentWidget->AddChild(this);
	}
	else if(ParentName != NAME_None)
	{
		ParentWidget = UWidgetModuleStatics::GetUserWidgetByName<UUserWidgetBase>(ParentName);
		if(ParentWidget)
		{
			ParentWidget->AddChild(this);
		}
	}

	if(WidgetOpenAnimator)
	{
		WidgetOpenAnimator->Execute_OnSpawn(WidgetOpenAnimator, this, {});
	}

	if(WidgetCloseAnimator)
	{
		WidgetCloseAnimator->Execute_OnSpawn(WidgetCloseAnimator, this, {});
	}

	K2_OnCreate(InOwner, InParams);

	for(const auto& Iter : UWidgetModuleStatics::GetUserWidgetChildrenByName(WidgetName))
	{
		if(UWidgetModuleStatics::HasUserWidgetClassByName(Iter))
		{
			const UUserWidgetBase* DefaultObject = UWidgetModuleStatics::GetUserWidgetClassByName(Iter)->GetDefaultObject<UUserWidgetBase>();
			if(DefaultObject->ParentName == WidgetName && (DefaultObject->WidgetCreateType == EWidgetCreateType::AutoCreate || DefaultObject->WidgetCreateType == EWidgetCreateType::AutoCreateAndOpen))
			{
				UWidgetModuleStatics::CreateUserWidgetByName<UUserWidgetBase>(Iter, InOwner);
			}
		}
	}

	for(const auto Iter : GetAllSubWidgets())
	{
		Iter->OnCreate(this, Iter->GetParams());
	}
}

void UUserWidgetBase::OnInitialize(UObject* InOwner, const TArray<FParameter>& InParams)
{
	WidgetParams = InParams;

	K2_OnInitialize(InOwner, InParams);

	for(auto& Iter : ChildWidgets)
	{
		Iter->OnInitialize(InOwner, InParams);
	}
}

void UUserWidgetBase::OnReset(bool bForce)
{
	K2_OnReset(bForce);
}

void UUserWidgetBase::OnOpen(const TArray<FParameter>& InParams, bool bInstant)
{
	if(WidgetState == EScreenWidgetState::Opening || WidgetState == EScreenWidgetState::Opened) return;
	
	WidgetParams = InParams;
	WidgetState = EScreenWidgetState::Opening;
	OnStateChanged(WidgetState);

	if(const auto ParentUserWidget = GetParentWidgetN<UUserWidgetBase>())
	{
		UPanelWidget* ParentSlotWidget;
		if(ParentSlot.IsNone())
		{
			ParentSlotWidget = ParentUserWidget->GetRootPanelWidget();
		}
		else
		{
			ParentSlotWidget = Cast<UPanelWidget>(ParentUserWidget->WidgetTree->FindWidget(ParentSlot));
		}
		if(ParentSlotWidget)
		{
			if(GetParent() != ParentSlotWidget)
			{
				UPanelSlot* PanelSlot = ParentSlotWidget->AddChild(this);
				if(UCanvasPanelSlot* CanvasPanelSlot = Cast<UCanvasPanelSlot>(PanelSlot))
				{
					CanvasPanelSlot->SetZOrder(WidgetZOrder);
					CanvasPanelSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
					CanvasPanelSlot->SetOffsets(FMargin(0.f));
				}
			}
		}
	}
	else
	{
		if(GetParent())
		{
			RemoveFromParent();
		}
		AddToViewport(WidgetZOrder);
	}
	switch(WidgetOpenType)
	{
		case EWidgetOpenType::Visible:
		{
			SetVisibility(ESlateVisibility::Visible);
			break;
		}
		case EWidgetOpenType::HitTestInvisible:
		{
			SetVisibility(ESlateVisibility::HitTestInvisible);
			break;
		}
		case EWidgetOpenType::SelfHitTestInvisible:
		{
			SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			break;
		}
		default: break;
	}
		
	Refresh();

	if(WidgetRefreshType == EWidgetRefreshType::Timer)
	{
		GetWorld()->GetTimerManager().SetTimer(WidgetRefreshTimerHandle, this, &UUserWidgetBase::Refresh, WidgetRefreshTime, true);
	}

	switch(WidgetOpenFinishType)
	{
		case EWidgetOpenFinishType::Instant:
		{
			FinishOpen(true);
			break;
		}
		case EWidgetOpenFinishType::Delay:
		{
			if(!bInstant)
			{
				FTimerDelegate TimerDelegate;
				TimerDelegate.BindUObject(this, &UUserWidgetBase::FinishOpen, false);
				GetWorld()->GetTimerManager().SetTimer(WidgetFinishOpenTimerHandle, TimerDelegate, WidgetRefreshTime, false);
			}
			else
			{
				FinishOpen(true);
			}
		}
		case EWidgetOpenFinishType::Animator:
		{
			if(WidgetOpenAnimator)
			{
				FOnWidgetAnimatorCompleted OnWidgetAnimatorCompleted;
				OnWidgetAnimatorCompleted.BindDynamic(this, &UUserWidgetBase::FinishOpen);
				WidgetOpenAnimator->Play(OnWidgetAnimatorCompleted, bInstant);
			}
			else
			{
				FinishOpen(true);
			}
		}
		default: break;
	}

	UInputModuleStatics::UpdateGlobalInputMode();

	if(K2_OnOpened.IsBound()) K2_OnOpened.Broadcast(InParams, bInstant);
	if(OnOpened.IsBound()) OnOpened.Broadcast(InParams, bInstant);

	UEventModuleStatics::BroadcastEvent<UEventHandle_UserWidgetOpened>(this, { this });

	K2_OnOpen(InParams, bInstant);

	for(const auto Iter : ChildWidgets)
	{
		if(Iter->GetParentName() == WidgetName && Iter->GetWidgetCreateType() == EWidgetCreateType::AutoCreateAndOpen)
		{
			Iter->Open(nullptr, bInstant);
		}
	}
}

void UUserWidgetBase::OnClose(bool bInstant)
{
	if(WidgetState == EScreenWidgetState::Closing || WidgetState == EScreenWidgetState::Closed) return;
	
	WidgetState = EScreenWidgetState::Closing;
	OnStateChanged(WidgetState);

	switch(WidgetCloseFinishType)
	{
		case EWidgetCloseFinishType::Instant:
		{
			FinishClose(true);
			break;
		}
		case EWidgetCloseFinishType::Delay:
		{
			if(!bInstant)
			{
				FTimerDelegate TimerDelegate;
				TimerDelegate.BindUObject(this, &UUserWidgetBase::FinishClose, false);
				GetWorld()->GetTimerManager().SetTimer(WidgetFinishCloseTimerHandle, TimerDelegate, WidgetCloseFinishTime, false);
			}
			else
			{
				FinishClose(true);
			}
		}
		case EWidgetCloseFinishType::Animator:
		{
			if(WidgetCloseAnimator)
			{
				FOnWidgetAnimatorCompleted OnWidgetAnimatorCompleted;
				OnWidgetAnimatorCompleted.BindDynamic(this, &UUserWidgetBase::FinishClose);
				WidgetCloseAnimator->Play(OnWidgetAnimatorCompleted, bInstant);
			}
			else
			{
				FinishClose(true);
			}
		}
		default: break;
	}

	if(K2_OnClosed.IsBound()) K2_OnClosed.Broadcast(bInstant);
	if(OnClosed.IsBound()) OnClosed.Broadcast(bInstant);

	UEventModuleStatics::BroadcastEvent<UEventHandle_UserWidgetClosed>(this, { this });

	K2_OnClose(bInstant);
}

void UUserWidgetBase::OnRefresh()
{
	K2_OnRefresh();
}

void UUserWidgetBase::OnDestroy(bool bRecovery)
{
	if(IsInViewport())
	{
		RemoveFromParent();
	}
	if(ParentWidget)
	{
		ParentWidget->RemoveChild(this);
	}

	UInputModuleStatics::UpdateGlobalInputMode();

	GetWorld()->GetTimerManager().ClearTimer(WidgetFinishOpenTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(WidgetFinishCloseTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(WidgetRefreshTimerHandle);

	UObjectPoolModuleStatics::DespawnObject(this, bRecovery);

	K2_OnDestroy(bRecovery);
}

void UUserWidgetBase::OnStateChanged(EScreenWidgetState InWidgetState)
{
	OnWidgetStateChanged.Broadcast(InWidgetState);

	K2_OnStateChanged(InWidgetState);
}

void UUserWidgetBase::Init(UObject* InOwner, const TArray<FParameter>* InParams, bool bForce)
{
	Init(InOwner, InParams ? *InParams : TArray<FParameter>(), bForce);
}

void UUserWidgetBase::Init(UObject* InOwner, const TArray<FParameter>& InParams, bool bForce)
{
	if(bForce || !InOwner || OwnerObject != InOwner)
	{
		OwnerObject = InOwner;

		OnInitialize(InOwner, InParams);
	}
}

void UUserWidgetBase::Open(const TArray<FParameter>* InParams, bool bInstant, bool bForce)
{
	UWidgetModuleStatics::OpenUserWidget<UUserWidgetBase>(InParams, bInstant, bForce, GetClass());
}

void UUserWidgetBase::Open(const TArray<FParameter>& InParams, bool bInstant, bool bForce)
{
	UWidgetModuleStatics::OpenUserWidget(GetClass(), InParams, bInstant, bForce);
}

void UUserWidgetBase::Close(bool bInstant)
{
	UWidgetModuleStatics::CloseUserWidget<UUserWidgetBase>(bInstant, GetClass());
}

void UUserWidgetBase::Toggle(bool bInstant)
{
	if(WidgetState == EScreenWidgetState::Opening || WidgetState == EScreenWidgetState::Closing) return;
	
	if(WidgetState != EScreenWidgetState::Opened)
	{
		Open(nullptr, bInstant);
	}
	else
	{
		Close(bInstant);
	}
}

void UUserWidgetBase::Reset(bool bForce)
{
	if(bForce)
	{
		OwnerObject = nullptr;
	}
	OnReset(bForce);
}

void UUserWidgetBase::Refresh()
{
	if(WidgetRefreshType == EWidgetRefreshType::None) return;

	OnRefresh();
}

void UUserWidgetBase::Destroy(bool bRecovery)
{
	UWidgetModuleStatics::DestroyUserWidget<UUserWidgetBase>(bRecovery, GetClass());
}

bool UUserWidgetBase::CanOpen_Implementation() const
{
	return true;
}

void UUserWidgetBase::FinishOpen(bool bInstant)
{
	if(WidgetState == EScreenWidgetState::Opened) return;

	WidgetState = EScreenWidgetState::Opened;
	OnStateChanged(WidgetState);

	if(IsFocusable())
	{
		SetFocus();
	}
}

void UUserWidgetBase::FinishClose(bool bInstant)
{
	if(WidgetState == EScreenWidgetState::Closed) return;

	WidgetState = EScreenWidgetState::Closed;
	OnStateChanged(WidgetState);

	switch(WidgetCloseType)
	{
		case EWidgetCloseType::Hidden:
		{
			SetVisibility(ESlateVisibility::Hidden);
			break;
		}
		case EWidgetCloseType::Collapsed:
		{
			SetVisibility(ESlateVisibility::Collapsed);
			break;
		}
		case EWidgetCloseType::Remove:
		{
			if(ParentWidget)
			{
				ParentWidget->RemoveChild(this);
			}
			RemoveFromParent();
			break;
		}
		default: break;
	}
	
	if(GetWidgetType(false) == EWidgetType::Temporary)
	{
		if(GetLastTemporary())
		{
			GetLastTemporary()->Open();
			SetLastTemporary(nullptr);
		}
	}

	if(WidgetRefreshType == EWidgetRefreshType::Timer)
	{
		GetWorld()->GetTimerManager().ClearTimer(WidgetRefreshTimerHandle);
	}

	UInputModuleStatics::UpdateGlobalInputMode();
}

USubWidgetBase* UUserWidgetBase::CreateSubWidget_Implementation(TSubclassOf<USubWidgetBase> InClass, const TArray<FParameter>& InParams)
{
	if(USubWidgetBase* SubWidget = UObjectPoolModuleStatics::SpawnObject<USubWidgetBase>(nullptr, nullptr, InClass))
	{
		SubWidget->OnCreate(this, InParams);
		return SubWidget;
	}
	return nullptr;
}

bool UUserWidgetBase::DestroySubWidget_Implementation(USubWidgetBase* InWidget, bool bRecovery)
{
	if(!InWidget) return false;

	InWidget->OnDestroy();

	UObjectPoolModuleStatics::DespawnObject(InWidget, bRecovery);
	return true;
}

TArray<USubWidgetBase*> UUserWidgetBase::GetAllSubWidgets() const
{
	TArray<USubWidgetBase*> SubWidgets;
	TArray<UWidget*> Widgets;
	UWidgetTree::GetChildWidgets(GetRootPanelWidget(), Widgets);
	for(auto Iter : Widgets)
	{
		if(USubWidgetBase* SubWidget = Cast<USubWidgetBase>(Iter))
		{
			SubWidgets.Add(SubWidget);
		}
	}
	return SubWidgets;
}

void UUserWidgetBase::AddChild(IScreenWidgetInterface* InChildWidget)
{
	if(!ChildWidgets.Contains(InChildWidget))
	{
		ChildWidgets.Add(InChildWidget);
	}
}

void UUserWidgetBase::RemoveChild(IScreenWidgetInterface* InChildWidget)
{
	if(ChildWidgets.Contains(InChildWidget))
	{
		ChildWidgets.Remove(InChildWidget);
	}
}

void UUserWidgetBase::RemoveAllChild()
{
	ChildWidgets.Empty();
}

UPanelWidget* UUserWidgetBase::GetRootPanelWidget() const
{
	return Cast<UPanelWidget>(GetRootWidget());
}
