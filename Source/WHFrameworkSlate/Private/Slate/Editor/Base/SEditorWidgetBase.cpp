// Fill out your copyright notice in the Description page of Project Settings.


#include "Slate/Editor/Base/SEditorWidgetBase.h"
#include "SlateOptMacros.h"
#include "Slate/SlateWidgetManager.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

const FName SEditorWidgetBase::WidgetName = NAME_None;

SEditorWidgetBase::SEditorWidgetBase()
	: WidgetCommands(new FUICommandList())
{
	WidgetType = EEditorWidgetType::Main;
	WidgetState = EEditorWidgetState::None;
	ChildWidgets = TArray<TSharedPtr<IEditorWidgetBase>>();
	ChildWidgetMap = TMap<FName, TSharedPtr<IEditorWidgetBase>>();

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

	if(WidgetState == EEditorWidgetState::None)
	{
		SetVisibility(EVisibility::Collapsed);
	}
	
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
	WidgetState = EEditorWidgetState::Opening;
	SetVisibility(EVisibility::SelfHitTestInvisible);
	if(bInstant)
	{
		FinishOpen(bInstant);
	}
	else
	{
		WidgetAnimSequence.Play(SharedThis(this));
	}
}

void SEditorWidgetBase::OnClose(bool bInstant)
{
	WidgetState = EEditorWidgetState::Closing;
	if(bInstant)
	{
		FinishClose(bInstant);
	}
	else
	{
		WidgetAnimSequence.Reverse();
	}
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

void SEditorWidgetBase::OnTick(float InDeltaTime)
{
	if(!WidgetAnimSequence.IsPlaying())
	{
		switch(WidgetState)
		{
			case EEditorWidgetState::Opening:
			{
				FinishOpen(false);
				break;
			}
			case EEditorWidgetState::Closing:
			{
				FinishClose(false);
				break;
			}
			default: break;
		}
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

void SEditorWidgetBase::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	OnTick(InDeltaTime);
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

void SEditorWidgetBase::FinishOpen(bool bInstant)
{
	WidgetState = EEditorWidgetState::Opened;
}

void SEditorWidgetBase::FinishClose(bool bInstant)
{
	WidgetState = EEditorWidgetState::Closed;
	SetVisibility(EVisibility::Collapsed);
}

bool SEditorWidgetBase::CanSave()
{
	return true;
}

void SEditorWidgetBase::AddChild(const TSharedPtr<IEditorWidgetBase>& InChildWidget)
{
	if(TSharedPtr<SEditorWidgetBase> ChildWidget = StaticCastSharedPtr<SEditorWidgetBase>(InChildWidget))
	{
		if(!ChildWidgets.Contains(ChildWidget))
		{
			ChildWidget->ParentWidget = SharedThis(this);
			ChildWidgets.Add(ChildWidget);
			ChildWidgetMap.Add(ChildWidget->WidgetName, ChildWidget);
		}
	}
}

void SEditorWidgetBase::RemoveChild(const TSharedPtr<IEditorWidgetBase>& InChildWidget)
{
	if(TSharedPtr<SEditorWidgetBase> ChildWidget = StaticCastSharedPtr<SEditorWidgetBase>(InChildWidget))
	{
		if(ChildWidgets.Contains(ChildWidget))
		{
			ChildWidget->ParentWidget = nullptr;
			ChildWidgets.Remove(ChildWidget);
			ChildWidgetMap.Remove(ChildWidget->WidgetName);
		}
	}
}

void SEditorWidgetBase::RemoveAllChild()
{
	for(auto Iter : ChildWidgets)
	{
		if(TSharedPtr<SEditorWidgetBase> ChildWidget = StaticCastSharedPtr<SEditorWidgetBase>(Iter))
		{
			ChildWidget->ParentWidget = nullptr;
		}
	}
	ChildWidgets.Empty();
	ChildWidgetMap.Empty();
}

TSharedPtr<SWindow> SEditorWidgetBase::GetOwnerWindow()
{
	return FSlateApplicationBase::Get().FindWidgetWindow(AsShared());
}

TSharedRef<IEditorWidgetBase> SEditorWidgetBase::TakeWidget()
{
	if(!bInitialized)
	{
		OnInitialize();
	}
	return SharedThis(this);
}

TSharedPtr<IEditorWidgetBase> SEditorWidgetBase::GetChild(int32 InIndex) const
{
	if(ChildWidgets.IsValidIndex(InIndex))
	{
		return ChildWidgets[InIndex];
	}
	return nullptr;
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
