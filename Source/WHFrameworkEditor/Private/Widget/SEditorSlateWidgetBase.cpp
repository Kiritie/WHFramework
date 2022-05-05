// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/SEditorSlateWidgetBase.h"
#include "SlateOptMacros.h"
#include "Main/MainModule.h"
#include "Widget/WidgetModule.h"
#include "Widget/WidgetModuleBPLibrary.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SEditorSlateWidgetBase::SEditorSlateWidgetBase()
{
	WidgetName = NAME_None;
	ChildWidgets = TArray<TSharedPtr<SEditorSlateWidgetBase>>();
}

void SEditorSlateWidgetBase::Construct(const FArguments& InArgs)
{
	OnCreate();
	/*
	ChildSlot
	[
		// Populate the widget
	];
	*/
}

void SEditorSlateWidgetBase::OnCreate()
{
	
}

void SEditorSlateWidgetBase::OnReset()
{
}

void SEditorSlateWidgetBase::OnRefresh()
{
	for(auto Iter : ChildWidgets)
	{
		Iter->OnRefresh();
	}
}

void SEditorSlateWidgetBase::OnDestroy()
{
}

void SEditorSlateWidgetBase::Reset()
{
	OnReset();
}

void SEditorSlateWidgetBase::Refresh()
{
	OnRefresh();
}

void SEditorSlateWidgetBase::Destroy()
{
	//UWidgetModuleBPLibrary::DestroySlateWidget<SEditorSlateWidgetBase>();
}

void SEditorSlateWidgetBase::AddChild(const TSharedPtr<SEditorSlateWidgetBase>& InChildWidget)
{
	if(!ChildWidgets.Contains(InChildWidget))
	{
		ChildWidgets.Add(InChildWidget);
	}
}

void SEditorSlateWidgetBase::RemoveChild(const TSharedPtr<SEditorSlateWidgetBase>& InChildWidget)
{
	if(ChildWidgets.Contains(InChildWidget))
	{
		ChildWidgets.Remove(InChildWidget);
	}
}

void SEditorSlateWidgetBase::RemoveAllChild()
{
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
