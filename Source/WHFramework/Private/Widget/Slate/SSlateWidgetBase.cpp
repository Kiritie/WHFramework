// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/Slate/SSlateWidgetBase.h"
#include "SlateOptMacros.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SSlateWidgetBase::SSlateWidgetBase()
{
	WidgetType = EWHWidgetType::None;
	InputMode = EWHInputMode::None;
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

void SSlateWidgetBase::OnInitialize()
{
}

void SSlateWidgetBase::OnAttach()
{
}

void SSlateWidgetBase::OnRefresh(float DeltaSeconds)
{
}

void SSlateWidgetBase::OnOpen()
{
}

void SSlateWidgetBase::OnClose()
{
}

void SSlateWidgetBase::OnDetach()
{
}

void SSlateWidgetBase::OnDestroy()
{
}

void SSlateWidgetBase::OpenSelf()
{
}

void SSlateWidgetBase::CloseSelf()
{
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
