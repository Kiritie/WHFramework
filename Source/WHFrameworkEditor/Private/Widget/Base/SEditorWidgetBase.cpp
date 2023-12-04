// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/Base/SEditorWidgetBase.h"
#include "SlateOptMacros.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SEditorWidgetBase::SEditorWidgetBase()
	: WidgetCommands(new FUICommandList())
{
	WidgetName = NAME_None;
	WidgetType = EEditorWidgetType::Main;
	ChildWidgets = TArray<TSharedPtr<SEditorWidgetBase>>();
	ChildWidgetMap = TMap<FName, TSharedPtr<SEditorWidgetBase>>();
}

void SEditorWidgetBase::Construct(const FArguments& InArgs)
{
	OnBindCommands();

	GWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateRaw(this, &SEditorWidgetBase::OnCreate));

	/*
	ChildSlot
	[
		// Populate the widget
	];
	*/
}

FReply SEditorWidgetBase::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if(WidgetCommands->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

void SEditorWidgetBase::OnWindowActivated()
{
}

void SEditorWidgetBase::OnWindowDeactivated()
{
}

void SEditorWidgetBase::OnWindowClosed(const TSharedRef<SWindow>& InOwnerWindow)
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

void SEditorWidgetBase::OnBindCommands()
{
	
}

void SEditorWidgetBase::OnCreate()
{
	if(WidgetType == EEditorWidgetType::Main)
	{
		if(GetOwnerWindow().IsValid())
		{
			OnWindowActivatedHandle = GetOwnerWindow()->GetOnWindowActivatedEvent().AddRaw(this, &SEditorWidgetBase::OnWindowActivated);
			OnWindowDeactivatedHandle = GetOwnerWindow()->GetOnWindowDeactivatedEvent().AddRaw(this, &SEditorWidgetBase::OnWindowDeactivated);
			OnWindowClosedHandle = GetOwnerWindow()->GetOnWindowClosedEvent().AddRaw(this, &SEditorWidgetBase::OnWindowClosed);
		}
	}
}

void SEditorWidgetBase::OnSave()
{
}

void SEditorWidgetBase::OnReset()
{
}

void SEditorWidgetBase::OnRefresh()
{
	for(auto Iter : ChildWidgets)
	{
		Iter->OnRefresh();
	}
}

void SEditorWidgetBase::OnDestroy()
{
	for(auto Iter : ChildWidgets)
	{
		Iter->OnDestroy();
	}
	ChildWidgets.Empty();
}

void SEditorWidgetBase::Save()
{
	OnSave();
}

void SEditorWidgetBase::Reset()
{
	OnReset();
}

void SEditorWidgetBase::Refresh()
{
	OnRefresh();
}

void SEditorWidgetBase::Destroy()
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

bool SEditorWidgetBase::CanSave()
{
	return true;
}

void SEditorWidgetBase::AddChild(const TSharedPtr<SEditorWidgetBase>& InChildWidget)
{
	if(!ChildWidgets.Contains(InChildWidget))
	{
		InChildWidget->ParentWidget = SharedThis(this);
		ChildWidgets.Add(InChildWidget);
		ChildWidgetMap.Add(InChildWidget->WidgetName, InChildWidget);
	}
}

void SEditorWidgetBase::RemoveChild(const TSharedPtr<SEditorWidgetBase>& InChildWidget)
{
	if(ChildWidgets.Contains(InChildWidget))
	{
		InChildWidget->ParentWidget = nullptr;
		ChildWidgets.Remove(InChildWidget);
		ChildWidgetMap.Remove(InChildWidget->WidgetName);
	}
}

void SEditorWidgetBase::RemoveAllChild()
{
	for(auto Iter : ChildWidgets)
	{
		Iter->ParentWidget = nullptr;
	}
	ChildWidgets.Empty();
	ChildWidgetMap.Empty();
}

TSharedPtr<SEditorWidgetBase> SEditorWidgetBase::GetChild(int32 InIndex) const
{
	if(ChildWidgets.IsValidIndex(InIndex))
	{
		return ChildWidgets[InIndex];
	}
	return nullptr;
}

TSharedPtr<SWindow> SEditorWidgetBase::GetOwnerWindow()
{
	return FSlateApplicationBase::Get().FindWidgetWindow(AsShared());
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
