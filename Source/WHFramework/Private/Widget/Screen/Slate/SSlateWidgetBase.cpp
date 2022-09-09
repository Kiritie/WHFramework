// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/Screen/Slate/SSlateWidgetBase.h"
#include "SlateOptMacros.h"
#include "Main/MainModule.h"
#include "Widget/WidgetModule.h"
#include "Widget/WidgetModuleBPLibrary.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SSlateWidgetBase::SSlateWidgetBase()
{
	WidgetType = EWidgetType::Default;
	WidgetCategory = EWidgetCategory::Permanent;
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
	WidgetCloseType = EWidgetCloseType::Hidden;
	WidgetRefreshType = EWidgetRefreshType::None;
	WidgetState = EScreenWidgetState::None;
	InputMode = EInputMode::None;
	OwnerActor = nullptr;
	LastWidget = nullptr;
	ParentWidget = nullptr;
	ChildWidgets = TArray<IScreenWidgetInterface*>();
}

void SSlateWidgetBase::Construct(const FArguments& InArgs)
{
	/*
	ChildSlot
	[
		// Populate the widget
	];
	*/
}

void SSlateWidgetBase::OnCreate(AActor* InOwner)
{
	
}

void SSlateWidgetBase::OnInitialize(AActor* InOwner)
{
	OwnerActor = InOwner;
}

void SSlateWidgetBase::OnOpen(const TArray<FParameter>& InParams, bool bInstant)
{
	WidgetState = EScreenWidgetState::Opening;
	
	switch (WidgetCategory)
	{
		case EWidgetCategory::Permanent:
		{
			SetVisibility(EVisibility::SelfHitTestInvisible);
		}
		case EWidgetCategory::Temporary:
		{
			//AddToViewport(WidgetZOrder);
		}
		default: break;
	}
	
	if(bInstant)
	{
		FinishOpen(bInstant);
	}
}

void SSlateWidgetBase::OnClose(bool bInstant)
{
	WidgetState = EScreenWidgetState::Closing;

	if(bInstant)
	{
		FinishClose(bInstant);
	}
}

void SSlateWidgetBase::OnReset()
{
}

void SSlateWidgetBase::OnRefresh()
{
}

void SSlateWidgetBase::OnDestroy(bool bRecovery)
{
}

void SSlateWidgetBase::OnStateChanged(EScreenWidgetState InWidgetChange)
{
}

void SSlateWidgetBase::Initialize(AActor* InOwner)
{
}

void SSlateWidgetBase::Open(const TArray<FParameter>* InParams, bool bInstant)
{
}

void SSlateWidgetBase::Open(const TArray<FParameter>& InParams, bool bInstant)
{
	//UWidgetModuleBPLibrary::OpenSlateWidget<SSlateWidgetBase>(InParams, bInstant);
}

void SSlateWidgetBase::Close(bool bInstant)
{
	//UWidgetModuleBPLibrary::CloseSlateWidget<SSlateWidgetBase>(bInstant);
}

void SSlateWidgetBase::Toggle(bool bInstant)
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

void SSlateWidgetBase::Reset()
{
	OnReset();
}

void SSlateWidgetBase::Refresh()
{
	if(WidgetState == EScreenWidgetState::Opened)
	{
		OnRefresh();
	}
}

void SSlateWidgetBase::Destroy(bool bRecovery)
{
	//UWidgetModuleBPLibrary::DestroySlateWidget<SSlateWidgetBase>();
}

void SSlateWidgetBase::FinishOpen(bool bInstant)
{
	WidgetState = EScreenWidgetState::Opened;

	Refresh();
}

void SSlateWidgetBase::FinishClose(bool bInstant)
{
	WidgetState = EScreenWidgetState::Closed;

	switch (WidgetCategory)
	{
		case EWidgetCategory::Permanent:
		{
			SetVisibility(EVisibility::Hidden);
			break;
		}
		case EWidgetCategory::Temporary:
		{
			if(!bInstant && GetLastWidget())
			{
				GetLastWidget()->Open();
			}
			//RemoveFromViewport();
		}
		default: break;
	}

	if(AInputModule* InputModule = AInputModule::Get())
	{
		InputModule->UpdateInputMode();
	}
}

void SSlateWidgetBase::AddChild(IScreenWidgetInterface* InChildWidget)
{
}

void SSlateWidgetBase::RemoveChild(IScreenWidgetInterface* InChildWidget)
{
}

void SSlateWidgetBase::RemoveAllChild()
{
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
