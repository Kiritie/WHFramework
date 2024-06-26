// Fill out your copyright notice in the Description page of Project Settings.


#include "Slate/Editor/Base/SEditorWidgetBase.h"
#include "SlateOptMacros.h"
#include "Slate/SlateWidgetManager.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

FName SEditorWidgetBase::WidgetName = NAME_None;

SEditorWidgetBase::SEditorWidgetBase()
	: WidgetCommands(new FUICommandList())
{
	WidgetType = EEditorWidgetType::Main;
	WidgetState = EEditorWidgetState::None;
	ChildWidgets = TArray<TSharedPtr<SEditorWidgetBase>>();
	ChildWidgetMap = TMap<FName, TSharedPtr<SEditorWidgetBase>>();

	bInitialized = false;
}

void SEditorWidgetBase::Construct(const FArguments& InArgs)
{
	OnCreate();
	
	/*
	ChildSlot
	[
		// Populate the widget
	];
	*/
}

void SEditorWidgetBase::OnCreate()
{
	
}

void SEditorWidgetBase::OnInitialize()
{
	bInitialized = true;
	
	SlatePrepass();

	OnBindCommands(WidgetCommands);
	
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

void SEditorWidgetBase::OnOpen(bool bInstant)
{
	WidgetState = EEditorWidgetState::Opened;
	SetVisibility(EVisibility::SelfHitTestInvisible);
}

void SEditorWidgetBase::OnClose(bool bInstant)
{
	WidgetState = EEditorWidgetState::Closed;
	SetVisibility(EVisibility::Collapsed);
}

void SEditorWidgetBase::OnSave()
{
	for(auto Iter : ChildWidgets)
	{
		Iter->OnSave();
	}
}

void SEditorWidgetBase::OnReset()
{
	for(auto Iter : ChildWidgets)
	{
		Iter->OnReset();
	}
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
	FSlateWidgetManager::Get().RemoveEditorWidget<SEditorWidgetBase>(GetWidgetName());

	switch(WidgetType)
	{
		case EEditorWidgetType::Main:
		{
			// if(GetOwnerWindow().IsValid())
			// {
			// 	GetOwnerWindow()->RequestDestroyWindow();
			// }
			break;
		}
		case EEditorWidgetType::Child:
		{
			break;
		}
	}

	for(auto Iter : ChildWidgets)
	{
		Iter->OnDestroy();
	}
	ChildWidgets.Empty();
}

void SEditorWidgetBase::OnBindCommands(const TSharedRef<FUICommandList>& InCommands)
{
	
}

void SEditorWidgetBase::OnWindowActivated()
{
}

void SEditorWidgetBase::OnWindowDeactivated()
{
}

void SEditorWidgetBase::OnWindowClosed(const TSharedRef<SWindow>& InOwnerWindow)
{
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

	Destroy();
}

FReply SEditorWidgetBase::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if(WidgetCommands->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

void SEditorWidgetBase::Open(bool bInstant)
{
	FSlateWidgetManager::Get().OpenEditorWidget<SEditorWidgetBase>(bInstant, GetWidgetName());
}

void SEditorWidgetBase::Close(bool bInstant)
{
	FSlateWidgetManager::Get().CloseEditorWidget<SEditorWidgetBase>(bInstant, GetWidgetName());
}

void SEditorWidgetBase::Toggle(bool bInstant)
{
	if(WidgetState == EEditorWidgetState::Opened)
	{
		Close(bInstant);
	}
	else
	{
		Open(bInstant);
	}
}

void SEditorWidgetBase::Save()
{
	OnSave();
}

void SEditorWidgetBase::Reset()
{
	OnReset();
}

void SEditorWidgetBase::Rebuild()
{
	OnDestroy();
	OnCreate();
}

void SEditorWidgetBase::Refresh()
{
	OnRefresh();
}

void SEditorWidgetBase::Destroy()
{
	FSlateWidgetManager::Get().DestroyEditorWidget<SEditorWidgetBase>(GetWidgetName());
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

TSharedPtr<SWindow> SEditorWidgetBase::GetOwnerWindow()
{
	return FSlateApplicationBase::Get().FindWidgetWindow(AsShared());
}

TSharedRef<SEditorWidgetBase> SEditorWidgetBase::TakeWidget()
{
	if(!bInitialized)
	{
		OnInitialize();
	}
	return SharedThis(this);
}

TSharedPtr<SEditorWidgetBase> SEditorWidgetBase::GetChild(int32 InIndex) const
{
	if(ChildWidgets.IsValidIndex(InIndex))
	{
		return ChildWidgets[InIndex];
	}
	return nullptr;
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
