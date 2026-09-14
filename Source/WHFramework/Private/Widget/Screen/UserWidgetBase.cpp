// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/Screen/UserWidgetBase.h"

#include "Input/Base/InputActionBase.h"
#include "Input/InputModule.h"
#include "Blueprint/WidgetTree.h"
#include "CommonInputBaseTypes.h"
#include "Input/CommonUIInputTypes.h"
#include "Components/PanelWidget.h"
#include "Components/WidgetSwitcher.h"
#include "Event/EventModuleStatics.h"
#include "Event/Events/Widget/Event_UserWidgetClosed.h"
#include "Event/Events/Widget/Event_UserWidgetCreated.h"
#include "Event/Events/Widget/Event_UserWidgetOpened.h"
#include "Event/Events/Widget/Event_UserWidgetStateChanged.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "Widget/WidgetModuleStatics.h"
#include "Slate/Runtime/Interfaces/SubWidgetInterface.h"
#include "Widget/Animator/WidgetAnimatorBase.h"
#include "Widget/WidgetModule.h"
#include "Widget/Screen/WidgetMountSlot.h"

namespace
{
	FParameter MakeScreenSubWidgetParam(
		UUserWidget* InOwnerWidget,
		const FParameter& InParam,
		bool bInDynamic)
	{
		if(!InParam.HasValue())
		{
			return FParameter(FSubWidgetSpawnParameter(InOwnerWidget, bInDynamic));
		}

		FParameter Param = InParam;
		if(FSubWidgetSpawnParameter* SpawnParam = Param.GetMutablePtr<FSubWidgetSpawnParameter>())
		{
			SpawnParam->OwnerObject = InOwnerWidget;
			SpawnParam->bDynamic = bInDynamic;
		}
		else
		{
			ensureEditorMsgf(
				false,
				FString::Printf(
					TEXT("Sub widget parameter %s must derive from FSubWidgetSpawnParameter."),
					*GetNameSafe(Param.GetValueStruct())),
				EDC_Widget,
				EDV_Error);
		}
		return Param;
	}
}

UUserWidgetBase::UUserWidgetBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bWidgetTickAble = false;

	WidgetOpenFinishType = EWidgetOpenFinishType::Instant;
	WidgetOpenFinishTime = 0.f;
	WidgetOpenAnimator = nullptr;
	WidgetCloseFinishType = EWidgetCloseFinishType::Instant;
	WidgetCloseFinishTime = 0.f;
	WidgetCloseAnimator = nullptr;
	WidgetRefreshType = EWidgetRefreshType::Procedure;
	WidgetRefreshTime = 0.f;
	WidgetState = EScreenWidgetState::None;
	InputConfig = EWidgetInputConfig::None;
	bWidgetActivatable = true;
	bWidgetAutoFocus = false;

	bInitialized = false;
	WidgetTag = FGameplayTag();
	ParentWidget = nullptr;
	SubWidgetEntries = TArray<FSubWidgetRuntimeEntry>();
	ChildWidgets = TArray<IScreenWidgetInterface*>();
}

const FScreenWidgetConfig* UUserWidgetBase::GetWidgetConfig() const
{
	return UWidgetModule::IsValid()
		? UWidgetModule::Get().GetUserWidgetConfig(WidgetTag)
		: nullptr;
}

EWidgetType UUserWidgetBase::GetWidgetType(bool bInheritParent) const
{
	if(bInheritParent && ParentWidget)
	{
		return ParentWidget->GetWidgetType();
	}
	const FScreenWidgetConfig* Config = GetWidgetConfig();
	return Config ? Config->WidgetType : EWidgetType::Permanent;
}

int32 UUserWidgetBase::GetWidgetZOrder() const
{
	const FScreenWidgetConfig* Config = GetWidgetConfig();
	return Config ? Config->ZOrder : 0;
}

FAnchors UUserWidgetBase::GetWidgetAnchors() const
{
	const FScreenWidgetConfig* Config = GetWidgetConfig();
	return Config ? Config->Anchors : FAnchors(0.f, 0.f, 1.f, 1.f);
}

bool UUserWidgetBase::IsWidgetAutoSize() const
{
	const FScreenWidgetConfig* Config = GetWidgetConfig();
	return Config ? Config->bAutoSize : false;
}

FVector2D UUserWidgetBase::GetWidgetDrawSize() const
{
	const FMargin Offsets = GetWidgetOffsets();
	return FVector2D(Offsets.Right, Offsets.Bottom);
}

FMargin UUserWidgetBase::GetWidgetOffsets() const
{
	const FScreenWidgetConfig* Config = GetWidgetConfig();
	return Config ? Config->Offsets : FMargin(0.f);
}

FVector2D UUserWidgetBase::GetWidgetAlignment() const
{
	const FScreenWidgetConfig* Config = GetWidgetConfig();
	return Config ? Config->Alignment : FVector2D::ZeroVector;
}

FUIActionBindingHandle UUserWidgetBase::RegisterUIActionByTag(
	FGameplayTag InActionTag,
	bool bInDisplayInActionBar)
{
	const UInputActionBase* InputAction = InActionTag.IsValid()
		? UInputModule::Get().GetInputActionByTag(InActionTag, false)
		: nullptr;
	return InputAction
		? RegisterUIActionBinding(FBindUIActionArgs(
			InputAction,
			bInDisplayInActionBar,
			FSimpleDelegate::CreateUObject(
				this,
				&ThisClass::HandleUIActionTriggered,
				InActionTag)))
		: FUIActionBindingHandle();
}

void UUserWidgetBase::OnUIActionTriggered_Implementation(FGameplayTag InActionTag)
{
}

void UUserWidgetBase::HandleUIActionTriggered(FGameplayTag InActionTag)
{
	OnUIActionTriggered(InActionTag);
}

void UUserWidgetBase::OnSpawn_Implementation(const FParameter& InParam)
{
	const FWidgetSpawnParameter* Param = InParam.GetPtr<FWidgetSpawnParameter>();
	OwnerObject = Param ? Param->OwnerObject.Get() : nullptr;
}

void UUserWidgetBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	OwnerObject = nullptr;
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

void UUserWidgetBase::OnCreate(const FParameter& InParam)
{
	if(UWidgetModule::IsValid()) UWidgetModule::Get().RegisterTickableWidget(this);
	RebuildWidgetMountSlotCache();

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

	const FParameter ChildParam = FParameter(FSubWidgetSpawnParameter(this, false));
	for(auto Iter : GetPoolWidgets())
	{
		IObjectPoolInterface::Execute_OnSpawn(Iter, ChildParam);
	}

	K2_OnCreate(InParam);

	UEventModuleStatics::BroadcastEvent<FEventUserWidgetCreated>(this, { this });

	TArray<UWidget*> Widgets;
	WidgetTree->GetAllWidgets(Widgets);
	for(auto Iter : Widgets)
	{
		if(ISubWidgetInterface* SubWidget = Cast<ISubWidgetInterface>(Iter))
		{
			FSubWidgetRuntimeEntry& Entry = SubWidgetEntries.AddDefaulted_GetRef();
			Entry.Widget = Cast<UUserWidget>(Iter);
			Entry.bDynamic = false;
			SubWidget->OnCreate(ChildParam);
		}
	}
}

void UUserWidgetBase::OnInitialize(const FParameter& InParam)
{
	InitializeParameter = InParam;
	K2_OnInitialize(InitializeParameter);
}

void UUserWidgetBase::OnReset(bool bForce)
{
	K2_OnReset(bForce);
}

void UUserWidgetBase::OnOpen(const FParameter& InParam, bool bInstant)
{
	if(const FWidgetOpenParameter* Param = InParam.GetPtr<FWidgetOpenParameter>();
		Param && Param->OwnerObject)
	{
		OwnerObject = Param->OwnerObject;
	}
	CurrentOpenParameter = InParam;
	AbortCloseTransition();

	if(WidgetState == EScreenWidgetState::Opening || WidgetState == EScreenWidgetState::Opened)
	{
		K2_OnOpen(CurrentOpenParameter, bInstant);
		return;
	}

	WidgetState = EScreenWidgetState::Opening;
	OnStateChanged(WidgetState);

	K2_OnOpen(CurrentOpenParameter, bInstant);

	SetVisibility(ESlateVisibility::Visible);
	if(bWidgetActivatable)
	{
		ActivateWidget();
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
			break;
		}
		default: break;
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
	if(!IsActivated()
		|| (WidgetState != EScreenWidgetState::Opening && WidgetState != EScreenWidgetState::Opened)
		|| GetVisibility() == ESlateVisibility::Collapsed
		|| GetVisibility() == ESlateVisibility::Hidden)
	{
		return false;
	}

	const UWidget* Child = this;
	for(const UPanelWidget* Parent = GetParent(); Parent; Parent = Parent->GetParent())
	{
		if(Parent->GetVisibility() == ESlateVisibility::Collapsed || Parent->GetVisibility() == ESlateVisibility::Hidden)
		{
			return false;
		}
		if(const UWidgetSwitcher* Switcher = Cast<UWidgetSwitcher>(Parent); Switcher && Switcher->GetActiveWidget() != Child)
		{
			return false;
		}
		Child = Parent;
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
			break;
		}
		default: break;
	}
}

void UUserWidgetBase::OnRefresh()
{
	K2_OnRefresh();
}

void UUserWidgetBase::OnDestroy(EObjectDespawnMode InMode)
{
	if(UWidgetModule::IsValid()) UWidgetModule::Get().UnregisterTickableWidget(this);

	AbortOpenTransition();
	AbortCloseTransition();
	GetWorld()->GetTimerManager().ClearTimer(WidgetRefreshTimerHandle);
	if(bWidgetActivatable)
	{
		DeactivateWidget();
	}
	DestroyAllSubWidget(InMode);

	for(UWidget* PoolWidget : GetPoolWidgets())
	{
		if(!PoolWidget->Implements<USubWidgetInterface>())
		{
			IObjectPoolInterface::Execute_OnDespawn(PoolWidget, InMode);
		}
	}
	if(WidgetOpenAnimator)
	{
		IObjectPoolInterface::Execute_OnDespawn(WidgetOpenAnimator, InMode);
	}
	if(WidgetCloseAnimator)
	{
		IObjectPoolInterface::Execute_OnDespawn(WidgetCloseAnimator, InMode);
	}

	K2_OnDestroy(InMode);
	if(K2_OnDestroyed.IsBound()) K2_OnDestroyed.Broadcast(InMode);
	if(OnDestroyed.IsBound()) OnDestroyed.Broadcast(InMode);

	bInitialized = false;
	WidgetState = EScreenWidgetState::None;
	WidgetTag = FGameplayTag();
	ParentWidget = nullptr;
	ChildWidgets.Reset();
	WidgetMountSlotMap.Reset();
	InitializeParameter.Reset();
	CurrentOpenParameter.Reset();
	UObjectPoolModuleStatics::DespawnObject(this, InMode);
}

void UUserWidgetBase::OnStateChanged(EScreenWidgetState InWidgetState)
{
	K2_OnStateChanged(InWidgetState);

	OnWidgetStateChanged.Broadcast(InWidgetState);

	UEventModuleStatics::BroadcastEvent<FEventUserWidgetStateChanged>(this, { this, InWidgetState });
}

void UUserWidgetBase::Init(const FParameter& InParam, bool bForce)
{
	if(!bInitialized || bForce)
	{
		OnInitialize(InParam);
		bInitialized = true;

		for(IScreenWidgetInterface* ChildWidget : ChildWidgets)
		{
			if(ChildWidget)
			{
				ChildWidget->Init(InParam, bForce);
			}
		}
	}
}

void UUserWidgetBase::Reset(bool bForce)
{
	OnReset(bForce);
}

void UUserWidgetBase::Open(const FParameter& InParam, bool bInstant, bool bForce)
{
	UWidgetModuleStatics::OpenUserWidgetByTag(WidgetTag, InParam, bInstant, bForce);
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
		Open(FWidgetOpenParameter(), bInstant);
	}
	else
	{
		Close(bInstant);
	}
}

void UUserWidgetBase::Refresh()
{
	if(WidgetRefreshType == EWidgetRefreshType::None || !IsWidgetActiveInHierarchy()) return;

	OnRefresh();
}

void UUserWidgetBase::Destroy(EObjectDespawnMode InMode)
{
	UWidgetModuleStatics::DestroyUserWidgetByTag(WidgetTag, InMode);
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

	if(bWidgetAutoFocus && IsWidgetActiveInHierarchy())
	{
		SetFocus();
	}

	if(K2_OnOpened.IsBound()) K2_OnOpened.Broadcast(CurrentOpenParameter, bInstant);
	if(OnOpened.IsBound()) OnOpened.Broadcast(CurrentOpenParameter, bInstant);

	UEventModuleStatics::BroadcastEvent<FEventUserWidgetOpened>(this, { this });

}

void UUserWidgetBase::FinishClose(bool bInstant)
{
	if(WidgetState != EScreenWidgetState::Closing) return;

	WidgetState = EScreenWidgetState::Closed;
	OnStateChanged(WidgetState);

	SetVisibility(ESlateVisibility::Hidden);
	
	if(WidgetRefreshType == EWidgetRefreshType::Timer)
	{
		GetWorld()->GetTimerManager().ClearTimer(WidgetRefreshTimerHandle);
	}

	if(bWidgetActivatable)
	{
		DeactivateWidget();
	}

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

UUserWidget* UUserWidgetBase::K2_CreateSubWidget(TSubclassOf<UUserWidget> InClass, const FParameter& InParam)
{
	return Cast<UUserWidget>(CreateSubWidget(InClass, InParam));
}

ISubWidgetInterface* UUserWidgetBase::CreateSubWidget(TSubclassOf<UUserWidget> InClass, const FParameter& InParam)
{
	const FParameter Param = MakeScreenSubWidgetParam(this, InParam, true);
	if(ISubWidgetInterface* SubWidget = Cast<ISubWidgetInterface>(
		UObjectPoolModuleStatics::SpawnObject(InClass.Get(), Param)))
	{
		FSubWidgetRuntimeEntry& Entry = SubWidgetEntries.AddDefaulted_GetRef();
		Entry.Widget = Cast<UUserWidget>(SubWidget);
		Entry.bDynamic = true;
		SubWidget->OnCreate(Param);
		return SubWidget;
	}
	return nullptr;
}

bool UUserWidgetBase::K2_DestroySubWidget(UUserWidget* InWidget, EObjectDespawnMode InMode)
{
	return DestroySubWidget(Cast<ISubWidgetInterface>(InWidget), InMode);
}

bool UUserWidgetBase::DestroySubWidget(ISubWidgetInterface* InWidget, EObjectDespawnMode InMode)
{
	if(!InWidget) return false;

	const int32 EntryIndex = FindSubWidget(InWidget);
	if(EntryIndex != INDEX_NONE)
	{
		const FSubWidgetRuntimeEntry Entry = SubWidgetEntries[EntryIndex];
		SubWidgetEntries.RemoveAt(EntryIndex);
		InWidget->OnDestroy(InMode);
		if(!Entry.bDynamic && Entry.Widget)
		{
			IObjectPoolInterface::Execute_OnDespawn(Entry.Widget, InMode);
		}
		return true;
	}
	InWidget->OnDestroy(InMode);
	return true;
}

void UUserWidgetBase::DestroyAllSubWidget(EObjectDespawnMode InMode)
{
	const TArray<FSubWidgetRuntimeEntry> Entries = MoveTemp(SubWidgetEntries);
	SubWidgetEntries.Reset();
	for(const FSubWidgetRuntimeEntry& Entry : Entries)
	{
		if(ISubWidgetInterface* SubWidget = Cast<ISubWidgetInterface>(Entry.Widget))
		{
			SubWidget->OnDestroy(InMode);
			if(!Entry.bDynamic)
			{
				IObjectPoolInterface::Execute_OnDespawn(Entry.Widget, InMode);
			}
		}
	}
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
	for(const FSubWidgetRuntimeEntry& Entry : SubWidgetEntries)
	{
		ReturnValues.Add(GetDeterminesOutputObject(Entry.Widget.Get(), InClass));
	}
	return ReturnValues;
}

TArray<ISubWidgetInterface*> UUserWidgetBase::GetSubWidgets()
{
	TArray<ISubWidgetInterface*> Result;
	Result.Reserve(SubWidgetEntries.Num());
	for(const FSubWidgetRuntimeEntry& Entry : SubWidgetEntries)
	{
		if(ISubWidgetInterface* SubWidget = Cast<ISubWidgetInterface>(Entry.Widget))
		{
			Result.Add(SubWidget);
		}
	}
	return Result;
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
