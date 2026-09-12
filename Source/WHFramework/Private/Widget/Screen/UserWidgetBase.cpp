// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/Screen/UserWidgetBase.h"

#include "Blueprint/WidgetTree.h"
#include "CommonInputBaseTypes.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ContentWidget.h"
#include "Components/PanelWidget.h"
#include "Event/EventModuleStatics.h"
#include "Event/Events/Widget/Event_UserWidgetClosed.h"
#include "Event/Events/Widget/Event_UserWidgetCreated.h"
#include "Event/Events/Widget/Event_UserWidgetOpened.h"
#include "Event/Events/Widget/Event_UserWidgetStateChanged.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "Widget/WidgetModuleStatics.h"
#include "Input/InputModuleStatics.h"
#include "Slate/Runtime/Interfaces/SubWidgetInterface.h"
#include "Widget/Animator/WidgetAnimatorBase.h"
#include "Widget/WidgetModule.h"
#include "Widget/Screen/WidgetMountSlot.h"

UUserWidgetBase::UUserWidgetBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bWidgetTickAble = false;

	WidgetType = EWidgetType::Permanent;
	WidgetZOrder = 0;
	WidgetAnchors = FAnchors(0.f, 0.f, 1.f, 1.f);
	bWidgetAutoSize = false;
	WidgetOffsets = FMargin(0.f);
	WidgetAlignment = FVector2D(0.f);
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
	InputConfig = EWidgetInputConfig::None;
	bWidgetAutoFocus = false;

	OwnerObject = nullptr;
	bInitialized = false;
	WidgetTag = FGameplayTag();
	ParentWidget = nullptr;
	SubWidgets = TArray<ISubWidgetInterface*>();
	ChildWidgets = TArray<IScreenWidgetInterface*>();
}

void UUserWidgetBase::OnSpawn_Implementation(const FParameter& InParam)
{
	
}

void UUserWidgetBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	
}

void UUserWidgetBase::OnTick_Implementation(float DeltaSeconds)
{

}

TOptional<FUIInputConfig> UUserWidgetBase::GetDesiredInputConfig() const
{
	switch(InputConfig)
	{
		case EWidgetInputConfig::Game:
			return FUIInputConfig(ECommonInputMode::Game, EMouseCaptureMode::CapturePermanently, true);
		case EWidgetInputConfig::GameAndMenu:
			return FUIInputConfig(ECommonInputMode::All, EMouseCaptureMode::NoCapture, false);
		case EWidgetInputConfig::Menu:
			return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture, false);
		default:
			return TOptional<FUIInputConfig>();
	}
}

void UUserWidgetBase::OnCreate(UObject* InOwner, const TArray<FParameter>& InParams)
{
	if(UWidgetModule::IsValid()) UWidgetModule::Get().RegisterTickableWidget(this);
	RebuildWidgetMountSlotCache();

	if(UUserWidgetBase* InParent = Cast<UUserWidgetBase>(InOwner); InParent != this)
	{
		ParentWidget = InParent;
		ParentWidget->AddChildWidget(this);
	}

	if(WidgetOpenAnimator)
	{
		IObjectPoolInterface::Execute_OnSpawn(
			WidgetOpenAnimator,
			FParameter(FWidgetSpawnParameter(this)));
	}

	if(WidgetCloseAnimator)
	{
		IObjectPoolInterface::Execute_OnSpawn(
			WidgetCloseAnimator,
			FParameter(FWidgetSpawnParameter(this)));
	}

	for(auto Iter : GetPoolWidgets())
	{
		IObjectPoolInterface::Execute_OnSpawn(Iter, FParameter(FWidgetSpawnParameter(this)));
	}

	K2_OnCreate(InOwner, InParams);

	UEventModuleStatics::BroadcastEvent<FEventUserWidgetCreated>(this, { this });

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
	CurrentOpenParameters = InParams;
	WidgetParams = InParams;
	AbortCloseTransition();

	if(WidgetState == EScreenWidgetState::Opening || WidgetState == EScreenWidgetState::Opened)
	{
		K2_OnOpen(CurrentOpenParameters, bInstant);
		return;
	}

	WidgetState = EScreenWidgetState::Opening;
	OnStateChanged(WidgetState);

	K2_OnOpen(CurrentOpenParameters, bInstant);
	ActivateWidget();

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
				GetWorld()->GetTimerManager().SetTimer(WidgetFinishOpenTimerHandle, TimerDelegate, WidgetOpenFinishTime, false);
			}
			else
			{
				FinishOpen(true);
			}
			break;
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

	switch(WidgetRefreshType)
	{
		case EWidgetRefreshType::Timer:
		{
			GetWorld()->GetTimerManager().SetTimer(WidgetRefreshTimerHandle, this, &UUserWidgetBase::Refresh, WidgetRefreshTime, true);
			break;
		}
		case EWidgetRefreshType::Procedure:
		{
			Refresh();
			break;
		}
		default: break;
	}

}

void UUserWidgetBase::RebuildWidgetMountSlotCache()
{
	WidgetMountSlotMap.Reset();
	if(!WidgetTree)
	{
		return;
	}

	TArray<UWidget*> Widgets;
	WidgetTree->GetAllWidgets(Widgets);
	for(UWidget* Widget : Widgets)
	{
		UWidgetMountSlot* MountSlot = Cast<UWidgetMountSlot>(Widget);
		if(!MountSlot || !MountSlot->GetSlotTag().IsValid())
		{
			continue;
		}

		ensureMsgf(
			!WidgetMountSlotMap.Contains(MountSlot->GetSlotTag()),
			TEXT("Duplicate WidgetMountSlot tag %s in %s"),
			*MountSlot->GetSlotTag().ToString(),
			*GetClass()->GetName());
		WidgetMountSlotMap.Add(MountSlot->GetSlotTag(), MountSlot);
	}
}

UWidgetMountSlot* UUserWidgetBase::GetWidgetMountSlot(const FGameplayTag& InSlotTag) const
{
	if(const TObjectPtr<UWidgetMountSlot>* MountSlot = WidgetMountSlotMap.Find(InSlotTag))
	{
		return MountSlot->Get();
	}
	return nullptr;
}

bool UUserWidgetBase::IsWidgetActiveInHierarchy() const
{
	if(!IsActivated() || WidgetState != EScreenWidgetState::Opened || GetVisibility() == ESlateVisibility::Collapsed || GetVisibility() == ESlateVisibility::Hidden)
	{
		return false;
	}
	const UUserWidgetBase* ParentUserWidget = GetParentWidgetN<UUserWidgetBase>();
	return !ParentUserWidget || ParentUserWidget->IsWidgetActiveInHierarchy();
}

void UUserWidgetBase::OnClose(bool bInstant)
{
	if(WidgetState == EScreenWidgetState::Closing || WidgetState == EScreenWidgetState::Closed) return;

	AbortOpenTransition();
	
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
			break;
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
}

void UUserWidgetBase::OnRefresh()
{
	K2_OnRefresh();
}

void UUserWidgetBase::OnDestroy(bool bRecovery)
{
	if(UWidgetModule::IsValid()) UWidgetModule::Get().UnregisterTickableWidget(this);

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

	AbortOpenTransition();
	AbortCloseTransition();
	GetWorld()->GetTimerManager().ClearTimer(WidgetRefreshTimerHandle);
	DeactivateWidget();

	if(K2_OnDestroyed.IsBound()) K2_OnDestroyed.Broadcast(bRecovery);
	if(OnDestroyed.IsBound()) OnDestroyed.Broadcast(bRecovery);

	UObjectPoolModuleStatics::DespawnObject(
		this,
		bRecovery ? EObjectDespawnMode::Recovery : EObjectDespawnMode::Destroy);

	OwnerObject = nullptr;
	bInitialized = false;
	WidgetParams.Empty();
	CurrentOpenParameters.Empty();
}

void UUserWidgetBase::OnStateChanged(EScreenWidgetState InWidgetState)
{
	K2_OnStateChanged(InWidgetState);

	OnWidgetStateChanged.Broadcast(InWidgetState);

	UEventModuleStatics::BroadcastEvent<FEventUserWidgetStateChanged>(this, { this, InWidgetState });
}

void UUserWidgetBase::Init(UObject* InOwner, const TArray<FParameter>* InParams, bool bForce)
{
	Init(InOwner, InParams ? *InParams : TArray<FParameter>(), bForce);
}

void UUserWidgetBase::Init(UObject* InOwner, const TArray<FParameter>& InParams, bool bForce)
{
	if(!bInitialized || OwnerObject != InOwner || bForce)
	{
		OnInitialize(InOwner, InParams);
		bInitialized = true;
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
	const FParameter Parameter = InParams && !InParams->IsEmpty()
		? (*InParams)[0]
		: FParameter();
	UWidgetModuleStatics::OpenUserWidgetByTag(WidgetTag, Parameter, bInstant, bForce);
}

void UUserWidgetBase::Open(const TArray<FParameter>& InParams, bool bInstant, bool bForce)
{
	const FParameter Parameter = InParams.IsEmpty() ? FParameter() : InParams[0];
	UWidgetModuleStatics::OpenUserWidgetByTag(WidgetTag, Parameter, bInstant, bForce);
}

void UUserWidgetBase::Close(bool bInstant)
{
	UWidgetModuleStatics::CloseUserWidgetByTag(WidgetTag, bInstant);
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
	UWidgetModuleStatics::DestroyUserWidgetByTag(WidgetTag, bRecovery);
}

bool UUserWidgetBase::CanOpen_Implementation() const
{
	return true;
}

void UUserWidgetBase::FinishOpen(bool bInstant)
{
	if(WidgetState != EScreenWidgetState::Opening) return;

	WidgetState = EScreenWidgetState::Opened;
	OnStateChanged(WidgetState);

	if(bWidgetAutoFocus)
	{
		SetFocus();
	}

	UInputModuleStatics::UpdateGlobalInputMode();

	if(K2_OnOpened.IsBound()) K2_OnOpened.Broadcast(CurrentOpenParameters, bInstant);
	if(OnOpened.IsBound()) OnOpened.Broadcast(CurrentOpenParameters, bInstant);

	UEventModuleStatics::BroadcastEvent<FEventUserWidgetOpened>(this, { this });

}

void UUserWidgetBase::FinishClose(bool bInstant)
{
	if(WidgetState != EScreenWidgetState::Closing) return;

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
	
	if(WidgetRefreshType == EWidgetRefreshType::Timer)
	{
		GetWorld()->GetTimerManager().ClearTimer(WidgetRefreshTimerHandle);
	}

	DeactivateWidget();

	UInputModuleStatics::UpdateGlobalInputMode();

	if(K2_OnClosed.IsBound()) K2_OnClosed.Broadcast(bInstant);
	if(OnClosed.IsBound()) OnClosed.Broadcast(bInstant);

	UEventModuleStatics::BroadcastEvent<FEventUserWidgetClosed>(this, { this });
}

void UUserWidgetBase::AbortOpenTransition()
{
	GetWorld()->GetTimerManager().ClearTimer(WidgetFinishOpenTimerHandle);
	if(WidgetOpenAnimator)
	{
		WidgetOpenAnimator->Abort();
	}
}

void UUserWidgetBase::AbortCloseTransition()
{
	GetWorld()->GetTimerManager().ClearTimer(WidgetFinishCloseTimerHandle);
	if(WidgetCloseAnimator)
	{
		WidgetCloseAnimator->Abort();
	}
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
	if(ISubWidgetInterface* SubWidget = Cast<ISubWidgetInterface>(
		UObjectPoolModuleStatics::SpawnObject(InClass.Get(), FParameter(FWidgetSpawnParameter(this)))))
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
	return GetParent();
}
