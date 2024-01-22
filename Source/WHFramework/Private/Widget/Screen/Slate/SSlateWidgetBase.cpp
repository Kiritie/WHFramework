// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/Screen/Slate/SSlateWidgetBase.h"
#include "SlateOptMacros.h"
#include "Input/InputModule.h"
#include "Widget/WidgetModule.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SSlateWidgetBase::SSlateWidgetBase()
{
	WidgetType = EWidgetType::Permanent;
	WidgetName = NAME_None;
	ParentName = NAME_None;
	ChildNames = TArray<FName>();
	WidgetZOrder = 0;
	WidgetAnchors = FAnchors(0.f, 0.f, 0.f, 0.f);
	bWidgetPenetrable = false;
	bWidgetAutoSize = false;
	WidgetDrawSize = FVector2D(0.f);
	WidgetOffsets = FMargin(0.f);
	WidgetAlignment = FVector2D(0.f);
	WidgetCreateType = EWidgetCreateType::None;
	WidgetOpenType = EWidgetOpenType::SelfHitTestInvisible;
	WidgetCloseType = EWidgetCloseType::Hidden;
	WidgetRefreshType = EWidgetRefreshType::None;
	WidgetState = EScreenWidgetState::None;
	WidgetInputMode = EInputMode::None;
	OwnerObject = nullptr;
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
	return !bWidgetPenetrable ? FReply::Handled() : SCompoundWidget::OnMouseButtonDown(MyGeometry, MouseEvent);
}

FReply SSlateWidgetBase::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return !bWidgetPenetrable ? FReply::Handled() : SCompoundWidget::OnMouseButtonUp(MyGeometry, MouseEvent);
}

FReply SSlateWidgetBase::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return !bWidgetPenetrable ? FReply::Handled() : SCompoundWidget::OnMouseWheel(MyGeometry, MouseEvent);
}

FReply SSlateWidgetBase::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return !bWidgetPenetrable ? FReply::Handled() : SCompoundWidget::OnMouseButtonDoubleClick(MyGeometry, MouseEvent);
}

FReply SSlateWidgetBase::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return !bWidgetPenetrable ? FReply::Handled() : SCompoundWidget::OnMouseMove(MyGeometry, MouseEvent);
}

FReply SSlateWidgetBase::OnTouchGesture(const FGeometry& MyGeometry, const FPointerEvent& GestureEvent)
{
	return !bWidgetPenetrable ? FReply::Handled() : SCompoundWidget::OnTouchGesture(MyGeometry, GestureEvent);
}

FReply SSlateWidgetBase::OnTouchStarted(const FGeometry& MyGeometry, const FPointerEvent& GestureEvent)
{
	return !bWidgetPenetrable ? FReply::Handled() : SCompoundWidget::OnTouchStarted(MyGeometry, GestureEvent);
}

FReply SSlateWidgetBase::OnTouchMoved(const FGeometry& MyGeometry, const FPointerEvent& GestureEvent)
{
	return !bWidgetPenetrable ? FReply::Handled() : SCompoundWidget::OnTouchMoved(MyGeometry, GestureEvent);
}

FReply SSlateWidgetBase::OnTouchEnded(const FGeometry& MyGeometry, const FPointerEvent& GestureEvent)
{
	return !bWidgetPenetrable ? FReply::Handled() : SCompoundWidget::OnTouchEnded(MyGeometry, GestureEvent);
}

void SSlateWidgetBase::OnCreate(UObject* InOwner, const TArray<FParameter>& InParams)
{
	
}

void SSlateWidgetBase::OnInitialize(UObject* InOwner, const TArray<FParameter>& InParams)
{
	OwnerObject = InOwner;
}

void SSlateWidgetBase::OnOpen(const TArray<FParameter>& InParams, bool bInstant)
{
	WidgetState = EScreenWidgetState::Opening;
	
	switch (WidgetType)
	{
		case EWidgetType::Permanent:
		{
			SetVisibility(EVisibility::SelfHitTestInvisible);
		}
		case EWidgetType::Temporary:
		{
			//AddToViewport(WidgetZOrder);
		}
		default: break;
	}
	
	if(bInstant)
	{
		FinishOpen(bInstant);
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

void SSlateWidgetBase::OnDestroy(bool bRecovery)
{
}

void SSlateWidgetBase::OnStateChanged(EScreenWidgetState InWidgetChange)
{
}

void SSlateWidgetBase::Init(UObject* InOwner, const TArray<FParameter>* InParams, bool bForce)
{
	Init(InOwner, InParams ? *InParams : TArray<FParameter>(), bForce);
}

void SSlateWidgetBase::Init(UObject* InOwner, const TArray<FParameter>& InParams, bool bForce)
{
	if(bForce || !InOwner || OwnerObject != InOwner)
	{
		OwnerObject = InOwner;

		OnInitialize(InOwner, InParams);
	}
}

void SSlateWidgetBase::Open(const TArray<FParameter>* InParams, bool bInstant, bool bForce)
{
}

void SSlateWidgetBase::Open(const TArray<FParameter>& InParams, bool bInstant, bool bForce)
{
	//UWidgetModuleStatics::OpenSlateWidget<SSlateWidgetBase>(InParams, bInstant);
}

void SSlateWidgetBase::Close(bool bInstant)
{
	//UWidgetModuleStatics::CloseSlateWidget<SSlateWidgetBase>(bInstant);
}

void SSlateWidgetBase::Toggle(bool bInstant)
{
	if(WidgetState == EScreenWidgetState::Opening || WidgetState == EScreenWidgetState::Closing) return;
	
	if(WidgetState != EScreenWidgetState::Opened)
	{
		Open(nullptr, bInstant);
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

void SSlateWidgetBase::Destroy(bool bRecovery)
{
	//UWidgetModuleStatics::DestroySlateWidget<SSlateWidgetBase>();
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
				GetLastTemporary()->Open();
			}
			//RemoveFromViewport();
		}
		default: break;
	}

	UInputModule::Get().UpdateInputMode();
}

void SSlateWidgetBase::AddChild(IScreenWidgetInterface* InChildWidget)
{
}

void SSlateWidgetBase::RemoveChild(IScreenWidgetInterface* InChildWidget)
{
}

void SSlateWidgetBase::RemoveAllChild()
{
}

UPanelWidget* SSlateWidgetBase::GetRootPanelWidget() const
{
	return nullptr;
}

UPanelWidget* SSlateWidgetBase::GetParentPanelWidget() const
{
	return nullptr;
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
