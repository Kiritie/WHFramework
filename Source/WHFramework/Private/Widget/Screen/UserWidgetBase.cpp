// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/Screen/UserWidgetBase.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ContentWidget.h"
#include "Components/PanelWidget.h"
#include "Event/EventModuleStatics.h"
#include "Event/Handle/Widget/EventHandle_UserWidgetClosed.h"
#include "Event/Handle/Widget/EventHandle_UserWidgetCreated.h"
#include "Event/Handle/Widget/EventHandle_UserWidgetOpened.h"
#include "Event/Handle/Widget/EventHandle_UserWidgetStateChanged.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "Widget/WidgetModuleStatics.h"
#include "Input/InputModuleStatics.h"
#include "Slate/Runtime/Interfaces/SubWidgetInterface.h"
#include "Widget/Animator/WidgetAnimatorBase.h"

UUserWidgetBase::UUserWidgetBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bWidgetTickAble = false;

	WidgetType = EWidgetType::Permanent;
	WidgetName = NAME_None;
	ParentName = NAME_None;
	ParentSlot = NAME_None;
	WidgetZOrder = 0;
	WidgetAnchors = FAnchors(0.f, 0.f, 1.f, 1.f);
	bWidgetAutoSize = false;
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
	bWidgetAutoFocus = false;

	OwnerObject = nullptr;
	LastTemporary = nullptr;
	ParentWidget = nullptr;
	TemporaryChild = nullptr;
	SubWidgets = TArray<ISubWidgetInterface*>();
	ChildWidgets = TArray<IScreenWidgetInterface*>();
}

void UUserWidgetBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	
}

void UUserWidgetBase::OnDespawn_Implementation(bool bRecovery)
{
	
}

void UUserWidgetBase::OnTick_Implementation(float DeltaSeconds)
{
	
}

void UUserWidgetBase::OnCreate(UObject* InOwner, const TArray<FParameter>& InParams)
{
	if(ParentWidget)
	{
		ParentWidget->RemoveChildWidget(this);
	}
	if(ParentName != NAME_None)
	{
		ParentWidget = UWidgetModuleStatics::GetUserWidgetByName<UUserWidgetBase>(ParentName);
	}
	else if(UUserWidgetBase* InParent = Cast<UUserWidgetBase>(InOwner); InParent != this)
	{
		ParentWidget = InParent;
	}
	if(ParentWidget)
	{
		ParentWidget->AddChildWidget(this);
	}

	if(WidgetOpenAnimator)
	{
		WidgetOpenAnimator->Execute_OnSpawn(WidgetOpenAnimator, this, {});
	}

	if(WidgetCloseAnimator)
	{
		WidgetCloseAnimator->Execute_OnSpawn(WidgetCloseAnimator, this, {});
	}

	for(auto Iter : GetPoolWidgets())
	{
		IObjectPoolInterface::Execute_OnSpawn(Iter, this, {});
	}

	K2_OnCreate(InOwner, InParams);

	UEventModuleStatics::BroadcastEvent<UEventHandle_UserWidgetCreated>(this, { this });

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
	
	TArray<UWidget*> Widgets;
	WidgetTree->GetAllWidgets(Widgets);
	for(auto Iter : Widgets)
	{
		if(ISubWidgetInterface* SubWidget = Cast<ISubWidgetInterface>(Iter))
		{
			SubWidgets.Add(SubWidget);
			SubWidget->OnCreate(this, SubWidget->GetWidgetParams());
		}
	}
}

void UUserWidgetBase::OnInitialize(UObject* InOwner, const TArray<FParameter>& InParams)
{
	OwnerObject = InOwner;

	WidgetParams = InParams;

	K2_OnInitialize(InOwner, InParams);
}

void UUserWidgetBase::OnReset(bool bForce)
{
	K2_OnReset(bForce);
}

void UUserWidgetBase::OnOpen(const TArray<FParameter>& InParams, bool bInstant)
{
	if(WidgetState == EScreenWidgetState::Opening || WidgetState == EScreenWidgetState::Opened) return K2_OnOpen(InParams, bInstant);
	
	WidgetParams = InParams;
	WidgetState = EScreenWidgetState::Opening;
	OnStateChanged(WidgetState);

	K2_OnOpen(InParams, bInstant);

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

	if(!GetParentWidgetN<UUserWidgetBase>())
	{
		if(GetParent())
		{
			RemoveFromParent();
		}
		AddToViewport(WidgetZOrder);
	}
	else if(UPanelWidget* ParentPanelWidget = GetParentPanelWidget())
	{
		if(GetParent() != ParentPanelWidget)
		{
			if(UContentWidget* ContentWidget = Cast<UContentWidget>(ParentPanelWidget))
			{
				ContentWidget->SetContent(this);
			}
			else if(UCanvasPanel* CanvasPanel = Cast<UCanvasPanel>(ParentPanelWidget))
			{
				UCanvasPanelSlot* CanvasPanelSlot = CanvasPanel->AddChildToCanvas(this);
				CanvasPanelSlot->SetZOrder(WidgetZOrder);
				CanvasPanelSlot->SetAnchors(WidgetAnchors);
				CanvasPanelSlot->SetOffsets(WidgetOffsets);
				CanvasPanelSlot->SetAlignment(WidgetAlignment);
			}
			else
			{
				ParentPanelWidget->AddChild(this);
			}
		}
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

	UInputModuleStatics::UpdateGlobalInputMode();

	if(K2_OnOpened.IsBound()) K2_OnOpened.Broadcast(InParams, bInstant);
	if(OnOpened.IsBound()) OnOpened.Broadcast(InParams, bInstant);

	UEventModuleStatics::BroadcastEvent<UEventHandle_UserWidgetOpened>(this, { this });

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

	K2_OnClose(bInstant);

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
}

void UUserWidgetBase::OnRefresh()
{
	K2_OnRefresh();
}

void UUserWidgetBase::OnDestroy(bool bRecovery)
{
	K2_OnDestroy(bRecovery);

	if(IsInViewport())
	{
		RemoveFromParent();
	}
	if(ParentWidget)
	{
		ParentWidget->RemoveChildWidget(this);
	}

	UInputModuleStatics::UpdateGlobalInputMode();

	GetWorld()->GetTimerManager().ClearTimer(WidgetFinishOpenTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(WidgetFinishCloseTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(WidgetRefreshTimerHandle);

	if(K2_OnDestroyed.IsBound()) K2_OnDestroyed.Broadcast(bRecovery);
	if(OnDestroyed.IsBound()) OnDestroyed.Broadcast(bRecovery);

	UObjectPoolModuleStatics::DespawnObject(this, bRecovery);

	OwnerObject = nullptr;
	WidgetParams.Empty();
}

void UUserWidgetBase::OnStateChanged(EScreenWidgetState InWidgetState)
{
	K2_OnStateChanged(InWidgetState);

	OnWidgetStateChanged.Broadcast(InWidgetState);

	UEventModuleStatics::BroadcastEvent<UEventHandle_UserWidgetStateChanged>(this, { this, &InWidgetState });
}

void UUserWidgetBase::Init(UObject* InOwner, const TArray<FParameter>* InParams, bool bForce)
{
	Init(InOwner, InParams ? *InParams : TArray<FParameter>(), bForce);
}

void UUserWidgetBase::Init(UObject* InOwner, const TArray<FParameter>& InParams, bool bForce)
{
	if(!InOwner || OwnerObject != InOwner || bForce)
	{
		OnInitialize(InOwner, InParams);
		
		for(auto& Iter : ChildWidgets)
		{
			Iter->Init(InOwner, InParams, bForce);
		}
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

void UUserWidgetBase::Open(const TArray<FParameter>* InParams, bool bInstant, bool bForce)
{
	UWidgetModuleStatics::OpenUserWidgetByName(WidgetName, InParams, bInstant, bForce);
}

void UUserWidgetBase::Open(const TArray<FParameter>& InParams, bool bInstant, bool bForce)
{
	UWidgetModuleStatics::OpenUserWidgetByName(WidgetName, InParams, bInstant, bForce);
}

void UUserWidgetBase::Close(bool bInstant)
{
	UWidgetModuleStatics::CloseUserWidgetByName(WidgetName, bInstant);
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

void UUserWidgetBase::Refresh()
{
	if(WidgetRefreshType == EWidgetRefreshType::None) return;

	OnRefresh();
}

void UUserWidgetBase::Destroy(bool bRecovery)
{
	UWidgetModuleStatics::DestroyUserWidgetByName(WidgetName, bRecovery);
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

	if(bWidgetAutoFocus)
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

UUserWidget* UUserWidgetBase::K2_CreateSubWidget(TSubclassOf<UUserWidget> InClass, const TArray<FParameter>& InParams)
{
	return Cast<UUserWidget>(CreateSubWidget(InClass, InParams));
}

ISubWidgetInterface* UUserWidgetBase::CreateSubWidget(TSubclassOf<UUserWidget> InClass, const TArray<FParameter>* InParams)
{
	return CreateSubWidget(InClass, InParams ? *InParams : TArray<FParameter>());
}

ISubWidgetInterface* UUserWidgetBase::CreateSubWidget(TSubclassOf<UUserWidget> InClass, const TArray<FParameter>& InParams)
{
	if(ISubWidgetInterface* SubWidget = UObjectPoolModuleStatics::SpawnObject<ISubWidgetInterface>(nullptr, nullptr, InClass))
	{
		SubWidget->OnCreate(this, InParams);
		return SubWidget;
	}
	return nullptr;
}

bool UUserWidgetBase::K2_DestroySubWidget(UUserWidget* InWidget, bool bRecovery)
{
	return DestroySubWidget(Cast<ISubWidgetInterface>(InWidget), bRecovery);
}

bool UUserWidgetBase::DestroySubWidget(ISubWidgetInterface* InWidget, bool bRecovery)
{
	if(!InWidget) return false;

	InWidget->OnDestroy(bRecovery);
	return true;
}

void UUserWidgetBase::DestroyAllSubWidget(bool bRecovery)
{
	for(auto Iter : SubWidgets)
	{
		Iter->Destroy();
	}
	SubWidgets.Empty();
}

void UUserWidgetBase::AddChildWidget(IScreenWidgetInterface* InWidget)
{
	if(!ChildWidgets.Contains(InWidget))
	{
		ChildWidgets.Add(InWidget);
	}
}

void UUserWidgetBase::RemoveChildWidget(IScreenWidgetInterface* InWidget)
{
	if(ChildWidgets.Contains(InWidget))
	{
		ChildWidgets.Remove(InWidget);
	}
}

void UUserWidgetBase::RemoveAllChildWidget()
{
	ChildWidgets.Empty();
}

TArray<UWidget*> UUserWidgetBase::GetPoolWidgets() const
{
	TArray<UWidget*> PoolWidgets;
	TArray<UWidget*> Widgets;
	WidgetTree->GetAllWidgets(Widgets);
	for(auto Iter : Widgets)
	{
		if(Iter->Implements<UObjectPoolInterface>())
		{
			PoolWidgets.Add(Iter);
		}
	}
	return PoolWidgets;
}

TArray<UUserWidget*> UUserWidgetBase::K2_GetSubWidgets(TSubclassOf<UUserWidget> InClass)
{
	TArray<UUserWidget*> ReturnValues;
	for(auto Iter : SubWidgets)
	{
		ReturnValues.Add(GetDeterminesOutputObject(Cast<UUserWidget>(Iter), InClass));
	}
	return ReturnValues;
}

UUserWidget* UUserWidgetBase::GetSubWidget(int32 InIndex, TSubclassOf<UUserWidget> InClass) const
{
	return GetDeterminesOutputObject(Cast<UUserWidget>(GetSubWidget(InIndex)), InClass);
}

int32 UUserWidgetBase::FindSubWidget(UUserWidget* InWidget) const
{
	return FindSubWidget(Cast<ISubWidgetInterface>(InWidget));
}

TArray<UUserWidgetBase*> UUserWidgetBase::K2_GetChildWidgets(TSubclassOf<UUserWidgetBase> InClass)
{
	TArray<UUserWidgetBase*> ReturnValues;
	for(auto Iter : ChildWidgets)
	{
		ReturnValues.Add(GetDeterminesOutputObject(Cast<UUserWidgetBase>(Iter), InClass));
	}
	return ReturnValues;
}

UWidgetAnimatorBase* UUserWidgetBase::GetWidgetOpenAnimator(TSubclassOf<UWidgetAnimatorBase> InClass) const
{
	return GetDeterminesOutputObject(WidgetOpenAnimator, InClass);
}

UWidgetAnimatorBase* UUserWidgetBase::GetWidgetCloseAnimator(TSubclassOf<UWidgetAnimatorBase> InClass) const
{
	return GetDeterminesOutputObject(WidgetCloseAnimator, InClass);
}

UPanelWidget* UUserWidgetBase::GetRootPanelWidget() const
{
	return Cast<UPanelWidget>(GetRootWidget());
}

UPanelWidget* UUserWidgetBase::GetParentPanelWidget() const
{
	if(const auto ParentUserWidget = GetParentWidgetN<UUserWidgetBase>())
	{
		return ParentSlot.IsNone() ? ParentUserWidget->GetRootPanelWidget() : Cast<UPanelWidget>(ParentUserWidget->WidgetTree->FindWidget(ParentSlot));
	}
	return nullptr;
}
