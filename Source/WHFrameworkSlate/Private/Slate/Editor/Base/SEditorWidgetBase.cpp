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

SEditorWidgetBase::~SEditorWidgetBase()
{
	DetachWindow();
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

    const TWeakPtr<SEditorWidgetBase> WeakThis = SharedThis(this);
    WindowBindTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis](float)
    {
        if (TSharedPtr<SEditorWidgetBase> Self = WeakThis.Pin())
        {
            Self->WindowBindTicker.Reset();
            if (Self->WidgetType == EEditorWidgetType::Main)
            {
                if (TSharedPtr<SWindow> Window = Self->GetOwnerWindow())
                {
                    Self->BoundWindow = Window;
                    Self->OnWindowActivatedHandle = Window->GetOnWindowActivatedEvent().AddRaw(Self.Get(), &SEditorWidgetBase::OnWindowActivated);
                    Self->OnWindowDeactivatedHandle = Window->GetOnWindowDeactivatedEvent().AddRaw(Self.Get(), &SEditorWidgetBase::OnWindowDeactivated);
                    Self->OnWindowClosedHandle = Window->GetOnWindowClosedEvent().AddRaw(Self.Get(), &SEditorWidgetBase::OnWindowClosed);
                }
            }
        }
        return false;
    }));
}

void SEditorWidgetBase::OnOpen(const FParameter& InParam, bool bInstant)
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

void SEditorWidgetBase::DetachWindow()
{
    if (WindowBindTicker.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(WindowBindTicker);
    WindowBindTicker.Reset();
    if (TSharedPtr<SWindow> Window = BoundWindow.Pin())
    {
        if (OnWindowActivatedHandle.IsValid()) Window->GetOnWindowActivatedEvent().Remove(OnWindowActivatedHandle);
        if (OnWindowDeactivatedHandle.IsValid()) Window->GetOnWindowDeactivatedEvent().Remove(OnWindowDeactivatedHandle);
        if (OnWindowClosedHandle.IsValid()) Window->GetOnWindowClosedEvent().Remove(OnWindowClosedHandle);
    }
    BoundWindow.Reset();
    OnWindowActivatedHandle.Reset();
    OnWindowDeactivatedHandle.Reset();
    OnWindowClosedHandle.Reset();
}

void SEditorWidgetBase::OnDestroy()
{
    DetachWindow();
    if (FSlateWidgetManager* Manager = FSlateWidgetManager::GetPtr()) Manager->RemoveEditorWidget<SEditorWidgetBase>(GetWidgetName());
    const TArray<TSharedPtr<IEditorWidgetBase>> Children = MoveTemp(ChildWidgets);
    ChildWidgetMap.Reset();
    for (const auto& Child : Children)
    {
        if (Child) Child->OnDestroy();
    }
    ParentWidget.Reset();
    bInitialized = false;
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
    DetachWindow();
    if (FSlateWidgetManager* Manager = FSlateWidgetManager::GetPtr()) Manager->DestroyEditorWidget<SEditorWidgetBase>(GetWidgetName());
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

void SEditorWidgetBase::Open(const FParameter& InParam, bool bInstant)
{
	FSlateWidgetManager::Get().OpenEditorWidget<SEditorWidgetBase>(InParam, bInstant, GetWidgetName());
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
		Open(FParameter(), bInstant);
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

void SEditorWidgetBase::AddChild(const TSharedPtr<IEditorWidgetBase>& InWidget)
{
	if(TSharedPtr<SEditorWidgetBase> ChildWidget = StaticCastSharedPtr<SEditorWidgetBase>(InWidget))
	{
		if(!ChildWidgets.Contains(ChildWidget))
		{
			ChildWidget->ParentWidget = SharedThis(this);
			ChildWidgets.Add(ChildWidget);
			ChildWidgetMap.Add(ChildWidget->WidgetName, ChildWidget);
		}
	}
}

void SEditorWidgetBase::RemoveChild(const TSharedPtr<IEditorWidgetBase>& InWidget)
{
	if(TSharedPtr<SEditorWidgetBase> ChildWidget = StaticCastSharedPtr<SEditorWidgetBase>(InWidget))
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
