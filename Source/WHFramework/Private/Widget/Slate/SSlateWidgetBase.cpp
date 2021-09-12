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

void SSlateWidgetBase::OnOpen()
{
	OnRefresh();
	SetVisibility(EVisibility::SelfHitTestInvisible);
}

void SSlateWidgetBase::OnClose()
{
	SetVisibility(EVisibility::Hidden);
}

void SSlateWidgetBase::OnToggle()
{
	if(GetVisibility() == EVisibility::Hidden)
	{
		OpenSelf();
	}
	else
	{
		CloseSelf();
	}
}

void SSlateWidgetBase::OnRefresh()
{
}

void SSlateWidgetBase::OnDestroy()
{
}

void SSlateWidgetBase::OpenSelf()
{
	UWidgetModuleBPLibrary::OpenSlateWidget<SSlateWidgetBase>(GWorld);
}

void SSlateWidgetBase::CloseSelf()
{
	UWidgetModuleBPLibrary::OpenSlateWidget<SSlateWidgetBase>(GWorld);
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
