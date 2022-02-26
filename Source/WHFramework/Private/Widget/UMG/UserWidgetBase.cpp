// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/UMG/UserWidgetBase.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Main/MainModule.h"
#include "Widget/WidgetModule.h"
#include "Widget/WidgetModuleBPLibrary.h"

UUserWidgetBase::UUserWidgetBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	WidgetName = NAME_None;
	ParentName = NAME_None;
	WidgetType = EWidgetType::None;
	WidgetOpenType = EWidgetOpenType::SelfHitTestInvisible;
	WidgetCloseType = EWidgetCloseType::Hidden;
	WidgetState = EWidgetState::None;
	InputMode = EInputMode::None;
	OwnerActor = nullptr;
	LastWidget = nullptr;
	ParentWidget = nullptr;
	ChildWidgets = TArray<TScriptInterface<IWidgetInterface>>();
}

void UUserWidgetBase::OnCreate_Implementation()
{
	if(WidgetType == EWidgetType::Child)
	{
		ParentWidget = UWidgetModuleBPLibrary::GetUserWidgetByName<UUserWidgetBase>(ParentName);
		ParentWidget->GetChildWidgets().Add(this);
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
		case EWidgetType::Permanent:
		case EWidgetType::Temporary:
		{
			if(!IsInViewport())
			{
				AddToViewport();
			}
			break;
		}
		case EWidgetType::Child:
		{
			if(!GetParent() && ParentWidget && ParentWidget->GetRootPanelWidget())
			{
				if(UCanvasPanelSlot* CanvasPanelSlot = Cast<UCanvasPanelSlot>(ParentWidget->GetRootPanelWidget()->AddChild(this)))
				{
					CanvasPanelSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
					CanvasPanelSlot->SetOffsets(FMargin(0.f));
				}
			}
			break;
		}
		default: break;
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

	if(bInstant)
	{
		FinishOpen(bInstant);
	}
}

void UUserWidgetBase::OnClose_Implementation(bool bInstant)
{
	WidgetState = EWidgetState::Closing;

	if(bInstant)
	{
		FinishClose(bInstant);
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
		ParentWidget->GetChildWidgets().Remove(this);
	}
}

void UUserWidgetBase::Open(TArray<FParameter>* InParams, bool bInstant)
{
	UWidgetModuleBPLibrary::OpenUserWidget<UUserWidgetBase>(InParams, bInstant, GetClass());
}

void UUserWidgetBase::Open_Implementation(const TArray<FParameter>& InParams, bool bInstant)
{
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
	if(WidgetState == EWidgetState::Opened)
	{
		OnRefresh();
	}
}

void UUserWidgetBase::Destroy_Implementation()
{
	UWidgetModuleBPLibrary::DestroyUserWidget<UUserWidgetBase>(GetClass());
}

void UUserWidgetBase::FinishOpen_Implementation(bool bInstant)
{
	WidgetState = EWidgetState::Opened;

	Refresh();
	
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
				case EWidgetType::Permanent:
				case EWidgetType::Temporary:
				{
					if(IsInViewport())
					{
						RemoveFromViewport();
					}
					break;
				}
				case EWidgetType::Child:
				{
					if(GetParent())
					{
						GetParent()->RemoveChild(this);
					}
					break;
				}
				default: break;
			}
			break;
		}
	}

	if(WidgetType == EWidgetType::Temporary)
	{
		if(!bInstant && GetLastWidget())
		{
			//GetLastWidget()->Open();
		}
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
