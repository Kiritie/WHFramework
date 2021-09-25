// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/Slate/SSlateWidgetBase.h"
#include "SlateOptMacros.h"
#include "WidgetModuleBPLibrary.h"

class AWidgetModule;
BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SSlateWidgetBase::SSlateWidgetBase()
{
	WidgetType = EWidgetType::None;
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

void SSlateWidgetBase::OnInitialize(AActor* InOwner)
{
	OwnerActor = InOwner;
}

void SSlateWidgetBase::OnOpen(bool bInstant)
{
	SetVisibility(EVisibility::SelfHitTestInvisible);
	Refresh();
}

void SSlateWidgetBase::OnClose(bool bInstant)
{
	SetVisibility(EVisibility::Hidden);
}

void SSlateWidgetBase::OnRefresh()
{
}

void SSlateWidgetBase::OnDestroy()
{
}

void SSlateWidgetBase::Open(bool bInstant)
{
	//UWidgetModuleBPLibrary::OpenSlateWidget<SSlateWidgetBase>(GWorld, bInstant);
	
	if(AWidgetModule* WidgetModule = UWidgetModuleBPLibrary::GetWidgetModule(GWorld))
	{
		WidgetModule->UpdateInputMode();
	}
}

void SSlateWidgetBase::Close(bool bInstant)
{
	//UWidgetModuleBPLibrary::OpenSlateWidget<SSlateWidgetBase>(GWorld, bInstant);
	
	if(AWidgetModule* WidgetModule = UWidgetModuleBPLibrary::GetWidgetModule(GWorld))
	{
		WidgetModule->UpdateInputMode();
	}
}

void SSlateWidgetBase::Toggle(bool bInstant)
{
	if(!IsOpened())
	{
		Open(bInstant);
	}
	else
	{
		Close(bInstant);
	}
}

void SSlateWidgetBase::Refresh()
{
	if(IsOpened())
	{
		OnRefresh();
	}
}

bool SSlateWidgetBase::IsOpened() const
{
	return GetVisibility() != EVisibility::Hidden && GetVisibility() != EVisibility::Collapsed;
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
