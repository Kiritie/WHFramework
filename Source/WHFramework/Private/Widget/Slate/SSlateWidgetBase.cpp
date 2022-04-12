// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/Slate/SSlateWidgetBase.h"
#include "SlateOptMacros.h"
#include "Main/MainModule.h"
#include "Widget/WidgetModule.h"
#include "Widget/WidgetModuleBPLibrary.h"

class AWidgetModule;
BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SSlateWidgetBase::SSlateWidgetBase()
{
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
	WidgetCloseType = EWidgetCloseType::Hidden;
	WidgetRefreshType = EWidgetRefreshType::None;
	WidgetState = EWidgetState::None;
	InputMode = EInputMode::None;
	OwnerActor = nullptr;
	LastWidget = nullptr;
	ParentWidget = nullptr;
	ChildWidgets = TArray<TScriptInterface<IWidgetInterface>>();
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

void SSlateWidgetBase::OnCreate()
{
	
}

void SSlateWidgetBase::OnInitialize(AActor* InOwner)
{
	OwnerActor = InOwner;
}

void SSlateWidgetBase::OnOpen(const TArray<FParameter>& InParams, bool bInstant)
{
	WidgetState = EWidgetState::Opening;
	
	switch (WidgetType)
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
	WidgetState = EWidgetState::Closing;

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

void SSlateWidgetBase::OnDestroy()
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

void SSlateWidgetBase::Reset()
{
	OnReset();
}

void SSlateWidgetBase::Refresh()
{
	if(WidgetState == EWidgetState::Opened)
	{
		OnRefresh();
	}
}

void SSlateWidgetBase::Destroy()
{
	//UWidgetModuleBPLibrary::DestroySlateWidget<SSlateWidgetBase>();
}

void SSlateWidgetBase::AddChild(const TScriptInterface<IWidgetInterface>& InChildWidget)
{
}

void SSlateWidgetBase::RemoveChild(const TScriptInterface<IWidgetInterface>& InChildWidget)
{
}

void SSlateWidgetBase::RemoveAllChild(const TScriptInterface<IWidgetInterface>& InChildWidget)
{
}

void SSlateWidgetBase::RefreshAllChild()
{
}

void SSlateWidgetBase::FinishOpen(bool bInstant)
{
	WidgetState = EWidgetState::Opened;

	Refresh();
}

void SSlateWidgetBase::FinishClose(bool bInstant)
{
	WidgetState = EWidgetState::Closed;

	switch (WidgetType)
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

	if(AInputModule* InputModule = AMainModule::GetModuleByClass<AInputModule>())
	{
		InputModule->UpdateInputMode();
	}
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
