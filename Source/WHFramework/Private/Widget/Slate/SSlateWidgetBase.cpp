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
	WidgetType = EWidgetType::None;
	WidgetState = EWidgetState::None;
	InputMode = EInputMode::None;
	OwnerActor = nullptr;
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
		case EWidgetType::Permanent:
		case EWidgetType::SemiPermanent:
		{
			SetVisibility(EVisibility::SelfHitTestInvisible);
		}
		case EWidgetType::Temporary:
		case EWidgetType::SemiTemporary:
		{
			//AddToViewport();
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

void SSlateWidgetBase::Open(bool bInstant)
{
	//UWidgetModuleBPLibrary::OpenSlateWidget<SSlateWidgetBase>(nullptr, bInstant);
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
		Open(bInstant);
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
		case EWidgetType::Permanent:
		case EWidgetType::SemiPermanent:
		{
			SetVisibility(EVisibility::Hidden);
			break;
		}
		case EWidgetType::Temporary:
		{
			if(!bInstant && GetLastWidget())
			{
				GetLastWidget()->Open();
			}
		}
		case EWidgetType::SemiTemporary:
		{
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
