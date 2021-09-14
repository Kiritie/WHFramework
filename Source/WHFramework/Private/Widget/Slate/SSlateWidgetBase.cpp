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
	if(bInstant)
	{
		SetVisibility(EVisibility::SelfHitTestInvisible);
	}
	OnRefresh();
}

void SSlateWidgetBase::OnClose(bool bInstant)
{
	if(bInstant)
	{
		SetVisibility(EVisibility::Hidden);
	}
}

void SSlateWidgetBase::OnToggle(bool bInstant)
{
	if(GetVisibility() == EVisibility::Hidden)
	{
		OpenSelf(bInstant);
	}
	else
	{
		CloseSelf(bInstant);
	}
}

void SSlateWidgetBase::OnRefresh()
{
}

void SSlateWidgetBase::OnDestroy()
{
}

void SSlateWidgetBase::OpenSelf(bool bInstant)
{
	UWidgetModuleBPLibrary::OpenSlateWidget<SSlateWidgetBase>(GWorld, bInstant);
}

void SSlateWidgetBase::CloseSelf(bool bInstant)
{
	UWidgetModuleBPLibrary::OpenSlateWidget<SSlateWidgetBase>(GWorld, bInstant);
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
