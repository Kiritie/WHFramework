// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/Screen/UMG/UserWidgetBase.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Main/MainModule.h"
#include "Widget/WidgetModule.h"
#include "Widget/WidgetModuleBPLibrary.h"

UUserWidgetBase::UUserWidgetBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bWidgetTickAble = false;
	WidgetType = EWidgetType::Default;
	WidgetCategory = EWidgetCategory::Permanent;
	WidgetName = NAME_None;
	ParentName = NAME_None;
	WidgetZOrder = 0;
	WidgetAnchors = FAnchors(0.f, 0.f, 0.f, 0.f);
	bWidgetAutoSize = false;
	WidgetDrawSize = FVector2D(0.f);
	WidgetOffsets = FMargin(0.f);
	WidgetAlignment = FVector2D(0.f);
	WidgetCreateType = EWidgetCreateType::None;
	WidgetOpenType = EWidgetOpenType::SelfHitTestInvisible;
	WidgetOpenFinishType = EWidgetOpenFinishType::Procedure;
	WidgetOpenFinishTime = 0.f;
	WidgetCloseType = EWidgetCloseType::Hidden;
	WidgetCloseFinishType = EWidgetCloseFinishType::Procedure;
	WidgetCloseFinishTime = 0.f;
	WidgetRefreshType = EWidgetRefreshType::Procedure;
	WidgetRefreshTime = 0.f;
	WidgetState = EWidgetState::None;
	InputMode = EInputMode::None;
	OwnerActor = nullptr;
	LastWidget = nullptr;
	ParentWidget = nullptr;
	ChildWidgets = TArray<TScriptInterface<IScreenWidgetInterface>>();
}

void UUserWidgetBase::OnTick_Implementation(float DeltaSeconds)
{
	
}

void UUserWidgetBase::OnCreate_Implementation()
{
	if(WidgetType == EWidgetType::Child && !ParentName.IsNone())
	{
		ParentWidget = UWidgetModuleBPLibrary::GetUserWidgetByName<UUserWidgetBase>(ParentName);
		if(ParentWidget)
		{
			ParentWidget->AddChild(this);
		}
	}
}

void UUserWidgetBase::OnInitialize_Implementation(AActor* InOwner)
{
	OwnerActor = InOwner;
}

void UUserWidgetBase::OnOpen_Implementation(const TArray<FParameter>& InParams, bool bInstant)
{
	WidgetState = EWidgetState::Opening;

	switch(WidgetType)
	{
		case EWidgetType::Child:
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
			break;
		}
		default:
		{
			if(!IsInViewport())
			{
				AddToViewport(WidgetZOrder);
			}
			break;
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
}

void UUserWidgetBase::OnClose_Implementation(bool bInstant)
{
	WidgetState = EWidgetState::Closing;

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

void UUserWidgetBase::OnDestroy_Implementation()
{
	if(IsInViewport())
	{
		RemoveFromViewport();
	}
	if(ParentWidget)
	{
		ParentWidget->RemoveChild(this);
	}
	if(AInputModule* InputModule = AMainModule::GetModuleByClass<AInputModule>())
	{
		InputModule->UpdateInputMode();
	}
	GetWorld()->GetTimerManager().ClearTimer(WidgetFinishOpenTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(WidgetFinishCloseTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(WidgetRefreshTimerHandle);
}

void UUserWidgetBase::Open(const TArray<FParameter>* InParams, bool bInstant)
{
	UWidgetModuleBPLibrary::OpenUserWidget<UUserWidgetBase>(InParams, bInstant, GetClass());
}

void UUserWidgetBase::Open_Implementation(const TArray<FParameter>& InParams, bool bInstant)
{
	if(WidgetState == EWidgetState::Opened) return;

	UWidgetModuleBPLibrary::K2_OpenUserWidget(GetClass(), InParams, bInstant);
}

void UUserWidgetBase::Close_Implementation(bool bInstant)
{
	UWidgetModuleBPLibrary::CloseUserWidget<UUserWidgetBase>(bInstant, GetClass());
}

void UUserWidgetBase::Toggle_Implementation(bool bInstant)
{
	if(WidgetState == EWidgetState::Opening || WidgetState == EWidgetState::Closing) return;
	
	if(WidgetState != EWidgetState::Opened)
	{
		Open(nullptr, bInstant);
	}
	else
	{
		Close(bInstant);
	}
}

void UUserWidgetBase::Reset_Implementation()
{
	OnReset();
}

void UUserWidgetBase::Refresh_Implementation()
{
	if(WidgetState != EWidgetState::Opened || WidgetRefreshType == EWidgetRefreshType::None) return;

	OnRefresh();
}

void UUserWidgetBase::Destroy_Implementation()
{
	UWidgetModuleBPLibrary::DestroyUserWidget<UUserWidgetBase>(GetClass());
}

void UUserWidgetBase::AddChild_Implementation(const TScriptInterface<IScreenWidgetInterface>& InChildWidget)
{
	if(!ChildWidgets.Contains(InChildWidget))
	{
		ChildWidgets.Add(InChildWidget);
	}
}

void UUserWidgetBase::RemoveChild_Implementation(const TScriptInterface<IScreenWidgetInterface>& InChildWidget)
{
	if(ChildWidgets.Contains(InChildWidget))
	{
		ChildWidgets.Remove(InChildWidget);
	}
}

void UUserWidgetBase::RemoveAllChild_Implementation(const TScriptInterface<IScreenWidgetInterface>& InChildWidget)
{
	ChildWidgets.Empty();
}

void UUserWidgetBase::RefreshAllChild_Implementation()
{
	for(auto Iter : ChildWidgets)
	{
		if(Iter)
		{
			Iter->Refresh();
		}
	}
}

void UUserWidgetBase::FinishOpen_Implementation(bool bInstant)
{
	WidgetState = EWidgetState::Opened;

	if(WidgetRefreshType == EWidgetRefreshType::Timer)
	{
		GetWorld()->GetTimerManager().SetTimer(WidgetRefreshTimerHandle, this, &UUserWidgetBase::Refresh, WidgetRefreshTime, true);
	}
	
	if(AInputModule* InputModule = AMainModule::GetModuleByClass<AInputModule>())
	{
		InputModule->UpdateInputMode();
	}
}

void UUserWidgetBase::FinishClose_Implementation(bool bInstant)
{
	WidgetState = EWidgetState::Closed;

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
			switch(WidgetType)
			{
				case EWidgetType::Child:
				{
					if(GetParent())
					{
						GetParent()->RemoveChild(this);
					}
					break;
				}
				default:
				{
					if(IsInViewport())
					{
						RemoveFromViewport();
					}
					break;
				}
			}
			break;
		}
	}

	if(!GetParentWidget() && GetWidgetCategory() == EWidgetCategory::Temporary)
	{
		if(!bInstant && GetLastWidget())
		{
			//GetLastWidget()->Open();
		}
	}

	if(WidgetRefreshType == EWidgetRefreshType::Timer)
	{
		GetWorld()->GetTimerManager().ClearTimer(WidgetRefreshTimerHandle);
	}

	if(AInputModule* InputModule = AMainModule::GetModuleByClass<AInputModule>())
	{
		InputModule->UpdateInputMode();
	}
}

UPanelWidget* UUserWidgetBase::GetRootPanelWidget() const
{
	return Cast<UPanelWidget>(GetRootWidget());
}
