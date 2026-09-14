// Fill out your copyright notice in the Description page of Project Settings.


#include "Slate/Runtime/Base/SSlateWidgetBase.h"
#include "SlateOptMacros.h"
#include "Engine/World.h"
#include "Slate/SlateWidgetManager.h"
#include "Slate/Runtime/Interfaces/SubWidgetInterface.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

const FName SSlateWidgetBase::WidgetName = NAME_None;
const FName SSlateWidgetBase::ParentName = NAME_None;

SSlateWidgetBase::SSlateWidgetBase()
{
	WidgetType = EWidgetType::Permanent;
	ParentSlot = NAME_None;
	WidgetZOrder = 0;
	WidgetAnchors = FAnchors(0.f, 0.f, 0.f, 0.f);
	bConsumePointerInput = false;
	bWidgetAutoSize = false;
	WidgetOffsets = FMargin(0.f);
	WidgetAlignment = FVector2D(0.f);
	WidgetCreateType = EWidgetCreateType::None;
	WidgetRefreshType = EWidgetRefreshType::None;
	WidgetState = EScreenWidgetState::None;
	WidgetInputMode = EInputMode::None;
	bInitialized = false;
	LastTemporary = nullptr;
	ParentWidget = nullptr;
	TemporaryChild = nullptr;
	ChildWidgets = TArray<IScreenWidgetInterface*>();
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

FReply SSlateWidgetBase::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return bConsumePointerInput ? FReply::Handled() : SCompoundWidget::OnMouseButtonDown(MyGeometry, MouseEvent);
}

FReply SSlateWidgetBase::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return bConsumePointerInput ? FReply::Handled() : SCompoundWidget::OnMouseButtonUp(MyGeometry, MouseEvent);
}

FReply SSlateWidgetBase::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return bConsumePointerInput ? FReply::Handled() : SCompoundWidget::OnMouseWheel(MyGeometry, MouseEvent);
}

FReply SSlateWidgetBase::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return bConsumePointerInput ? FReply::Handled() : SCompoundWidget::OnMouseButtonDoubleClick(MyGeometry, MouseEvent);
}

FReply SSlateWidgetBase::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return bConsumePointerInput ? FReply::Handled() : SCompoundWidget::OnMouseMove(MyGeometry, MouseEvent);
}

FReply SSlateWidgetBase::OnTouchGesture(const FGeometry& MyGeometry, const FPointerEvent& GestureEvent)
{
	return bConsumePointerInput ? FReply::Handled() : SCompoundWidget::OnTouchGesture(MyGeometry, GestureEvent);
}

FReply SSlateWidgetBase::OnTouchStarted(const FGeometry& MyGeometry, const FPointerEvent& GestureEvent)
{
	return bConsumePointerInput ? FReply::Handled() : SCompoundWidget::OnTouchStarted(MyGeometry, GestureEvent);
}

FReply SSlateWidgetBase::OnTouchMoved(const FGeometry& MyGeometry, const FPointerEvent& GestureEvent)
{
	return bConsumePointerInput ? FReply::Handled() : SCompoundWidget::OnTouchMoved(MyGeometry, GestureEvent);
}

FReply SSlateWidgetBase::OnTouchEnded(const FGeometry& MyGeometry, const FPointerEvent& GestureEvent)
{
	return bConsumePointerInput ? FReply::Handled() : SCompoundWidget::OnTouchEnded(MyGeometry, GestureEvent);
}

void SSlateWidgetBase::OnCreate(const FParameter& InParam)
{
	WidgetParams = InParam;

	if(ParentWidget)
	{
		ParentWidget->RemoveChildWidget(this);
	}

	if(ParentName != NAME_None)
	{
		// ParentWidget = FSlateWidgetManager::Get().GetSlateWidget<SSlateWidgetBase>(ParentName);
	}
	if(ParentWidget)
	{
		ParentWidget->AddChildWidget(this);
	}
	//
	// for(const auto& Iter : FSlateWidgetManager::Get().GetUserWidgetChildrenByName(WidgetName))
	// {
	// 	if(FSlateWidgetManager::Get().HasUserWidgetClassByName(Iter))
	// 	{
	// 		const SSlateWidgetBase* DefaultObject = FSlateWidgetManager::Get().GetUserWidgetClassByName(Iter)->GetDefaultObject<SSlateWidgetBase>();
	// 		if(DefaultObject->ParentName == WidgetName && (DefaultObject->WidgetCreateType == EWidgetCreateType::AutoCreate || DefaultObject->WidgetCreateType == EWidgetCreateType::AutoCreateAndOpen))
	// 		{
	// 			FSlateWidgetManager::Get().CreateUserWidgetByName<SSlateWidgetBase>(Iter, OwnerObject);
	// 		}
	// 	}
	// }
}

void SSlateWidgetBase::OnInitialize(const FParameter& InParam)
{
	WidgetParams = InParam;
}

void SSlateWidgetBase::OnOpen(const FParameter& InParam, bool bInstant)
{
	if(WidgetState == EScreenWidgetState::Opening || WidgetState == EScreenWidgetState::Opened) return;
	
	WidgetParams = InParam;
	WidgetState = EScreenWidgetState::Opening;
	OnStateChanged(WidgetState);

	if(!GetParentWidget())
	{
		GWorld->GetGameViewport()->AddViewportWidgetContent(SharedThis(this), WidgetZOrder);
	}

	SetVisibility(EVisibility::Visible);
		
	Refresh();

	if(WidgetRefreshType == EWidgetRefreshType::Timer)
	{
		// UCommonModuleStatics::GetCurrentTimerManager()->GetTimerManager().SetTimer(WidgetRefreshTimerHandle, this, &SSlateWidgetBase::Refresh, WidgetRefreshTime, true);
	}
	if(bInstant)
	{
		FinishOpen(bInstant);
	}

	// if(K2_OnOpened.IsBound()) K2_OnOpened.Broadcast(InParam, bInstant);
	// if(OnOpened.IsBound()) OnOpened.Broadcast(InParam, bInstant);

	for(const auto Iter : ChildWidgets)
	{
		const SSlateWidgetBase* ChildWidget = static_cast<const SSlateWidgetBase*>(Iter);
		if(ChildWidget->GetParentName() == GetWidgetName()
			&& ChildWidget->GetWidgetCreateType() == EWidgetCreateType::AutoCreateAndOpen)
		{
			Iter->Open(FParameter(), bInstant);
		}
	}
}

void SSlateWidgetBase::OnClose(bool bInstant)
{
	WidgetState = EScreenWidgetState::Closing;

	if(bInstant)
	{
		FinishClose(bInstant);
	}
}

void SSlateWidgetBase::OnReset(bool bForce)
{
}

void SSlateWidgetBase::OnRefresh()
{
}

void SSlateWidgetBase::OnDestroy(EObjectDespawnMode InMode)
{
	if(GWorld->GetGameViewport())
	{
		GWorld->GetGameViewport()->RemoveViewportWidgetContent(SharedThis(this));
	}

	if(ParentWidget)
	{
		ParentWidget->RemoveChildWidget(this);
	}

	bInitialized = false;
	WidgetParams.Reset();
}

void SSlateWidgetBase::OnStateChanged(EScreenWidgetState InWidgetChange)
{
}

void SSlateWidgetBase::Init(const FParameter& InParam, bool bForce)
{
	if(!bInitialized || bForce)
	{
		OnInitialize(InParam);
		bInitialized = true;
	}
}

void SSlateWidgetBase::Open(const FParameter& InParam, bool bInstant, bool bForce)
{
	FSlateWidgetManager::Get().OpenSlateWidget<SSlateWidgetBase>(InParam, bInstant, GetWidgetName());
}

void SSlateWidgetBase::Close(bool bInstant)
{
	FSlateWidgetManager::Get().CloseSlateWidget<SSlateWidgetBase>(bInstant, GetWidgetName());
}

void SSlateWidgetBase::Toggle(bool bInstant)
{
	if(WidgetState == EScreenWidgetState::Opening || WidgetState == EScreenWidgetState::Closing) return;
	
	if(WidgetState != EScreenWidgetState::Opened)
	{
		Open(FParameter(), bInstant);
	}
	else
	{
		Close(bInstant);
	}
}

void SSlateWidgetBase::Reset(bool bForce)
{
	OnReset(bForce);
}

void SSlateWidgetBase::Refresh()
{
	if(WidgetState == EScreenWidgetState::Opened)
	{
		OnRefresh();
	}
}

void SSlateWidgetBase::Destroy(EObjectDespawnMode InMode)
{
	FSlateWidgetManager::Get().DestroySlateWidget<SSlateWidgetBase>(InMode, GetWidgetName());
}

bool SSlateWidgetBase::CanOpen() const
{
	return true;
}

void SSlateWidgetBase::FinishOpen(bool bInstant)
{
	WidgetState = EScreenWidgetState::Opened;

	Refresh();
}

void SSlateWidgetBase::FinishClose(bool bInstant)
{
	WidgetState = EScreenWidgetState::Closed;

	switch (WidgetType)
	{
		case EWidgetType::Permanent:
		{
			SetVisibility(EVisibility::Hidden);
			break;
		}
		case EWidgetType::Temporary:
		{
			if(!bInstant && GetLastTemporary())
			{
				GetLastTemporary()->Open(FParameter());
			}
			GWorld->GetGameViewport()->RemoveViewportWidgetContent(SharedThis(this));
		}
		default: break;
	}

}

ISubWidgetInterface* SSlateWidgetBase::CreateSubWidget(TSubclassOf<UUserWidget> InClass, const FParameter& InParam)
{
	return nullptr;
}

bool SSlateWidgetBase::DestroySubWidget(ISubWidgetInterface* InWidget, EObjectDespawnMode InMode)
{
	if(!InWidget) return false;

	InWidget->OnDestroy(InMode);

	return true;
}

void SSlateWidgetBase::DestroyAllSubWidget(EObjectDespawnMode InMode)
{
	for(auto Iter : SubWidgets)
	{
		Iter->Destroy(InMode);
	}
	SubWidgets.Empty();
}

void SSlateWidgetBase::AddChildWidget(IScreenWidgetInterface* InWidget)
{
	if(!ChildWidgets.Contains(InWidget))
	{
		ChildWidgets.Add(InWidget);
	}
}

void SSlateWidgetBase::RemoveChildWidget(IScreenWidgetInterface* InWidget)
{
	if(ChildWidgets.Contains(InWidget))
	{
		ChildWidgets.Remove(InWidget);
	}
}

void SSlateWidgetBase::RemoveAllChildWidget()
{
	ChildWidgets.Empty();
}

TArray<UWidget*> SSlateWidgetBase::GetPoolWidgets() const
{
	TArray<UWidget*> PoolWidgets;
	return PoolWidgets;
}

TSharedPtr<SPanel> SSlateWidgetBase::GetRootPanelWidget() const
{
	return nullptr;
}

TSharedPtr<SPanel> SSlateWidgetBase::GetParentPanelWidget() const
{
	return nullptr;
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
