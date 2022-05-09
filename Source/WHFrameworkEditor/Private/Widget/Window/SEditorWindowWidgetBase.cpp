// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/Window/SEditorWindowWidgetBase.h"

#define LOCTEXT_NAMESPACE "EditorWindowWidgetBase"

SEditorWindowWidgetBase::SEditorWindowWidgetBase()
{
	WindowTitle = LOCTEXT("EditorWindowWidget_Title", "EditorWindowWidget");
	WindowSize = FVector2D(450, 450);
}

void SEditorWindowWidgetBase::Construct(const FArguments& InArgs)
{
	auto WindowArgs = InArgs._WindowArgs;
	WindowArgs.Title_Lambda([this](){ return WindowTitle; });
	WindowArgs.SupportsMinimize(false);
	WindowArgs.SupportsMaximize(false);
	WindowArgs.ClientSize(WindowSize);
	SWindow::Construct(WindowArgs);

	OnCreate();
}

void SEditorWindowWidgetBase::OnWindowActivated()
{
	
}

void SEditorWindowWidgetBase::OnWindowDeactivated()
{
	
}

void SEditorWindowWidgetBase::OnWindowClosed(const TSharedRef<SWindow>& InWindow)
{
	if(OnWindowActivatedHandle.IsValid())
	{
		GetOnWindowActivatedEvent().Remove(OnWindowActivatedHandle);
	}
	if(OnWindowDeactivatedHandle.IsValid())
	{
		GetOnWindowDeactivatedEvent().Remove(OnWindowDeactivatedHandle);
	}
	if(OnWindowClosedHandle.IsValid())
	{
		GetOnWindowClosedEvent().Remove(OnWindowClosedHandle);
	}
}

void SEditorWindowWidgetBase::OnCreate()
{
	OnWindowActivatedHandle = GetOnWindowActivatedEvent().AddRaw(this, &SEditorWindowWidgetBase::OnWindowActivated);
	OnWindowDeactivatedHandle = GetOnWindowDeactivatedEvent().AddRaw(this, &SEditorWindowWidgetBase::OnWindowDeactivated);
	OnWindowClosedHandle = GetOnWindowClosedEvent().AddRaw(this, &SEditorWindowWidgetBase::OnWindowClosed);
}

void SEditorWindowWidgetBase::OnReset()
{
	
}

void SEditorWindowWidgetBase::OnRefresh()
{
	
}

void SEditorWindowWidgetBase::OnDestroy()
{

}

void SEditorWindowWidgetBase::Reset()
{
	OnReset();
}

void SEditorWindowWidgetBase::Refresh()
{
	OnRefresh();
}

void SEditorWindowWidgetBase::Destroy()
{
	FSlateApplicationBase::Get().RequestDestroyWindow(SharedThis(this));
}

#undef LOCTEXT_NAMESPACE
