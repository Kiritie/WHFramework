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
	WidgetType = EEditorWidgetType::Main;
	ChildWidgets = TArray<TSharedPtr<SEditorSlateWidgetBase>>();
	ChildWidgetMap = TMap<FName, TSharedPtr<SEditorSlateWidgetBase>>();
}

void SEditorSlateWidgetBase::Construct(const FArguments& InArgs)
{
	GWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateRaw(this, &SEditorSlateWidgetBase::OnCreate));
	/*
	ChildSlot
	[
		// Populate the widget
	];
	*/
}

void SEditorSlateWidgetBase::OnWindowActivated()
{
}

void SEditorSlateWidgetBase::OnWindowDeactivated()
{
}

void SEditorSlateWidgetBase::OnWindowClosed(const TSharedRef<SWindow>& InOwnerWindow)
{
	OnDestroy();
	if(OnWindowActivatedHandle.IsValid())
	{
		InOwnerWindow->GetOnWindowActivatedEvent().Remove(OnWindowActivatedHandle);
	}
	if(OnWindowDeactivatedHandle.IsValid())
	{
		InOwnerWindow->GetOnWindowDeactivatedEvent().Remove(OnWindowDeactivatedHandle);
	}
	if(OnWindowClosedHandle.IsValid())
	{
		InOwnerWindow->GetOnWindowClosedEvent().Remove(OnWindowClosedHandle);
	}
}

void SEditorSlateWidgetBase::OnCreate()
{
	if(WidgetType == EEditorWidgetType::Main)
	{
		if(GetOwnerWindow().IsValid())
		{
			OnWindowActivatedHandle = GetOwnerWindow()->GetOnWindowActivatedEvent().AddRaw(this, &SEditorSlateWidgetBase::OnWindowActivated);
			OnWindowDeactivatedHandle = GetOwnerWindow()->GetOnWindowDeactivatedEvent().AddRaw(this, &SEditorSlateWidgetBase::OnWindowDeactivated);
			OnWindowClosedHandle = GetOwnerWindow()->GetOnWindowClosedEvent().AddRaw(this, &SEditorSlateWidgetBase::OnWindowClosed);
		}
	}
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
	for(auto Iter : ChildWidgets)
	{
		Iter->OnDestroy();
	}
	ChildWidgets.Empty();
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
	switch(WidgetType)
	{
		case EEditorWidgetType::Main:
		{
			if(GetOwnerWindow().IsValid())
			{
				GetOwnerWindow()->RequestDestroyWindow();
			}
			break;
		}
		case EEditorWidgetType::Child:
		{
			break;
		}
	}
}

void SEditorSlateWidgetBase::AddChild(const TSharedPtr<SEditorSlateWidgetBase>& InChildWidget)
{
	if(!ChildWidgets.Contains(InChildWidget))
	{
		InChildWidget->ParentWidget = SharedThis(this);
		ChildWidgets.Add(InChildWidget);
		ChildWidgetMap.Add(InChildWidget->WidgetName, InChildWidget);
	}
}

void SEditorSlateWidgetBase::RemoveChild(const TSharedPtr<SEditorSlateWidgetBase>& InChildWidget)
{
	if(ChildWidgets.Contains(InChildWidget))
	{
		InChildWidget->ParentWidget = nullptr;
		ChildWidgets.Remove(InChildWidget);
		ChildWidgetMap.Remove(InChildWidget->WidgetName);
	}
}

void SEditorSlateWidgetBase::RemoveAllChild()
{
	for(auto Iter : ChildWidgets)
	{
		Iter->ParentWidget = nullptr;
	}
	ChildWidgets.Empty();
	ChildWidgetMap.Empty();
}

TSharedPtr<SWindow> SEditorSlateWidgetBase::GetOwnerWindow()
{
	return FSlateApplicationBase::Get().FindWidgetWindow(AsShared());
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
