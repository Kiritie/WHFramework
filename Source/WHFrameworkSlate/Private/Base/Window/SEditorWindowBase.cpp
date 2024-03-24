// Fill out your copyright notice in the Description page of Project Settings.


#include "Base/Window/SEditorWindowBase.h"

#define LOCTEXT_NAMESPACE "EditorWindowBase"

SEditorWindowBase::SEditorWindowBase()
{
	WindowTitle = LOCTEXT("EditorWindowWidget_Title", "EditorWindowWidget");
	WindowSize = FVector2D(450, 450);
}

void SEditorWindowBase::Construct(const FArguments& InArgs)
{
	auto WindowArgs = InArgs._WindowArgs;
	WindowArgs.Title_Lambda([this](){ return WindowTitle; });
	WindowArgs.SupportsMinimize(false);
	WindowArgs.SupportsMaximize(false);
	WindowArgs.ClientSize(WindowSize);
	SWindow::Construct(WindowArgs);

	OnCreate();
}

void SEditorWindowBase::OnWindowActivated()
{
	
}

void SEditorWindowBase::OnWindowDeactivated()
{
	
}

void SEditorWindowBase::OnWindowClosed(const TSharedRef<SWindow>& InWindow)
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

void SEditorWindowBase::OnCreate()
{
	OnWindowActivatedHandle = GetOnWindowActivatedEvent().AddRaw(this, &SEditorWindowBase::OnWindowActivated);
	OnWindowDeactivatedHandle = GetOnWindowDeactivatedEvent().AddRaw(this, &SEditorWindowBase::OnWindowDeactivated);
	OnWindowClosedHandle = GetOnWindowClosedEvent().AddRaw(this, &SEditorWindowBase::OnWindowClosed);
}

void SEditorWindowBase::OnReset()
{
	
}

void SEditorWindowBase::OnRefresh()
{
	
}

void SEditorWindowBase::OnDestroy()
{

}

void SEditorWindowBase::Reset()
{
	OnReset();
}

void SEditorWindowBase::Refresh()
{
	OnRefresh();
}

void SEditorWindowBase::Destroy()
{
	FSlateApplicationBase::Get().RequestDestroyWindow(SharedThis(this));
}

#undef LOCTEXT_NAMESPACE
