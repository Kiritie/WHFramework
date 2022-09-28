// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/Screen/UMG/UserWidgetBase.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Main/MainModule.h"
#include "ObjectPool/ObjectPoolModuleBPLibrary.h"
#include "Widget/WidgetModule.h"
#include "Widget/WidgetModuleBPLibrary.h"

UUserWidgetBase::UUserWidgetBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bWidgetTickAble = false;
	WidgetType = EWidgetType::Permanent;
	WidgetName = NAME_None;
	ParentName = NAME_None;
	ChildNames = TArray<FName>();
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
	WidgetCloseType = EWidgetCloseType::Hidden;
	WidgetCloseFinishType = EWidgetCloseFinishType::Instant;
	WidgetCloseFinishTime = 0.f;
	WidgetRefreshType = EWidgetRefreshType::Procedure;
	WidgetRefreshTime = 0.f;
	WidgetState = EScreenWidgetState::None;
	WidgetParams = TArray<FParameter>();
	InputMode = EInputMode::None;
	OwnerActor = nullptr;
	LastTemporary = nullptr;
	ParentWidget = nullptr;
	TemporaryChild = nullptr;
	ChildWidgets = TArray<IScreenWidgetInterface*>();
}

void UUserWidgetBase::OnTick_Implementation(float DeltaSeconds)
{
	
}

void UUserWidgetBase::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	
}

void UUserWidgetBase::OnDespawn_Implementation()
{
	
}

void UUserWidgetBase::OnCreate_Implementation(AActor* InOwner)
{
	if(ParentName != NAME_None)
	{
		ParentWidget = UWidgetModuleBPLibrary::GetUserWidgetByName<UUserWidgetBase>(ParentName);
		if(ParentWidget)
		{
			ParentWidget->AddChild(this);
		}
	}

	for(auto Iter : ChildNames)
	{
		if(UWidgetModuleBPLibrary::HasUserWidgetClassByName(Iter))
		{
			const UUserWidgetBase* DefaultObject = UWidgetModuleBPLibrary::GetUserWidgetClassByName(Iter)->GetDefaultObject<UUserWidgetBase>();
			if(DefaultObject->ParentName != NAME_None && (DefaultObject->WidgetCreateType == EWidgetCreateType::AutoCreate || DefaultObject->WidgetCreateType == EWidgetCreateType::AutoCreateAndOpen))
			{
				UWidgetModuleBPLibrary::CreateUserWidgetByName<UUserWidgetBase>(Iter, InOwner);
			}
		}
	}
}

void UUserWidgetBase::OnInitialize_Implementation(AActor* InOwner)
{
	OwnerActor = InOwner;
}

void UUserWidgetBase::OnOpen_Implementation(const TArray<FParameter>& InParams, bool bInstant)
{
	if(WidgetState == EScreenWidgetState::Opening || WidgetState == EScreenWidgetState::Opened) return;
	
	WidgetParams = InParams;
	WidgetState = EScreenWidgetState::Opening;
	OnStateChanged(WidgetState);

	if(ParentName != NAME_None)
	{
		if(!GetParent() && ParentWidget && ParentWidget->GetRootPanelWidget())
		{
			if(UCanvasPanelSlot* CanvasPanelSlot = Cast<UCanvasPanelSlot>(ParentWidget->GetRootPanelWidget()->AddChild(this)))
			{
				CanvasPanelSlot->SetZOrder(WidgetZOrder);
				CanvasPanelSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
				CanvasPanelSlot->SetOffsets(FMargin(0.f));
			}
		}
	}
	else
	{
		if(!IsInViewport())
		{
			AddToViewport(WidgetZOrder);
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
	}
		
	Refresh();

	if(WidgetRefreshType == EWidgetRefreshType::Timer)
	{
		GetWorld()->GetTimerManager().SetTimer(WidgetRefreshTimerHandle, this, &UUserWidgetBase::Refresh, WidgetRefreshTime, true);
	}

	if(bInstant || WidgetOpenFinishType == EWidgetOpenFinishType::Instant)
	{
		FinishOpen(bInstant);
	}
	else if(WidgetOpenFinishType == EWidgetOpenFinishType::Delay)
	{
		FTimerDelegate TimerDelegate;
		TimerDelegate.BindUObject(this, &UUserWidgetBase::FinishOpen, bInstant);
		GetWorld()->GetTimerManager().SetTimer(WidgetFinishOpenTimerHandle, TimerDelegate, WidgetRefreshTime, false);
	}

	UInputModuleBPLibrary::UpdateGlobalInputMode();

	for(auto Iter : ChildNames)
	{
		if(UWidgetModuleBPLibrary::HasUserWidgetClassByName(Iter))
		{
			const UUserWidgetBase* DefaultObject = Cast<UUserWidgetBase>(UWidgetModuleBPLibrary::GetUserWidgetClassByName(Iter)->GetDefaultObject());
			if(DefaultObject->ParentName != NAME_None && DefaultObject->WidgetCreateType == EWidgetCreateType::AutoCreateAndOpen)
			{
				UWidgetModuleBPLibrary::OpenUserWidgetByName(Iter);
			}
		}
	}
}

void UUserWidgetBase::OnClose_Implementation(bool bInstant)
{
	if(WidgetState == EScreenWidgetState::Closing || WidgetState == EScreenWidgetState::Closed) return;
	
	WidgetState = EScreenWidgetState::Closing;
	OnStateChanged(WidgetState);

	if(bInstant || WidgetCloseFinishType == EWidgetCloseFinishType::Instant)
	{
		FinishClose(bInstant);
	}
	else if(WidgetCloseFinishType == EWidgetCloseFinishType::Delay)
	{
		FTimerDelegate TimerDelegate;
		TimerDelegate.BindUObject(this, &UUserWidgetBase::FinishClose, bInstant);
		GetWorld()->GetTimerManager().SetTimer(WidgetFinishCloseTimerHandle, TimerDelegate, WidgetCloseFinishTime, false);
	}
}

void UUserWidgetBase::OnReset_Implementation()
{
	
}

void UUserWidgetBase::OnRefresh_Implementation()
{
	
}

void UUserWidgetBase::OnDestroy_Implementation(bool bRecovery)
{
	if(IsInViewport())
	{
		RemoveFromViewport();
	}
	if(ParentWidget)
	{
		ParentWidget->RemoveChild(this);
	}

	UInputModuleBPLibrary::UpdateGlobalInputMode();

	GetWorld()->GetTimerManager().ClearTimer(WidgetFinishOpenTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(WidgetFinishCloseTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(WidgetRefreshTimerHandle);

	if(bRecovery)
	{
		UObjectPoolModuleBPLibrary::DespawnObject(this);
	}
	else
	{
		ConditionalBeginDestroy();
	}
}

void UUserWidgetBase::OnStateChanged_Implementation(EScreenWidgetState InWidgetState)
{
	OnWidgetStateChanged.Broadcast(InWidgetState);
}

bool UUserWidgetBase::Initialize()
{
	return Super::Initialize();
}

void UUserWidgetBase::Initialize(AActor* InOwner)
{
	UWidgetModuleBPLibrary::InitializeUserWidget<UUserWidgetBase>(InOwner, GetClass());
}

void UUserWidgetBase::Open(const TArray<FParameter>* InParams, bool bInstant)
{
	UWidgetModuleBPLibrary::OpenUserWidget<UUserWidgetBase>(InParams, bInstant, GetClass());
}

void UUserWidgetBase::Open(const TArray<FParameter>& InParams, bool bInstant)
{
	UWidgetModuleBPLibrary::OpenUserWidget(GetClass(), InParams, bInstant);
}

void UUserWidgetBase::Close(bool bInstant)
{
	UWidgetModuleBPLibrary::CloseUserWidget<UUserWidgetBase>(bInstant, GetClass());
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

void UUserWidgetBase::Reset()
{
	OnReset();
}

void UUserWidgetBase::Refresh()
{
	if(WidgetRefreshType == EWidgetRefreshType::None) return;

	OnRefresh();
}

void UUserWidgetBase::Destroy(bool bRecovery)
{
	UWidgetModuleBPLibrary::DestroyUserWidget<UUserWidgetBase>(bRecovery, GetClass());
}

void UUserWidgetBase::FinishOpen(bool bInstant)
{
	if(WidgetState == EScreenWidgetState::Opened) return;

	WidgetState = EScreenWidgetState::Opened;
	OnStateChanged(WidgetState);

	if(bIsFocusable)
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
		if(ParentName != NAME_None)
		{
			if(GetParent())
			{
				GetParent()->RemoveChild(this);
			}
		}
		else
		{
			if(IsInViewport())
			{
				RemoveFromViewport();
			}
		}
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

	UInputModuleBPLibrary::UpdateGlobalInputMode();
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
