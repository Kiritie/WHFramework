// Fill out your copyright notice in the Description page of Project Settings.


#include "Slate/Runtime/Base/SSlateWidgetBase.h"
#include "SlateOptMacros.h"
#if WITH_ENGINE
#include "Engine/World.h"
#endif
#include "Input/InputManager.h"
#include "Slate/SlateWidgetManager.h"

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

void SSlateWidgetBase::OnCreate(UObject* InOwner, const TArray<FParameter>& InParams)
{
	WidgetParams = InParams;

	if(ParentWidget)
	{
		ParentWidget->RemoveChild(this);
	}
	if(ParentName != NAME_None)
	{
		// ParentWidget = FSlateWidgetManager::Get().GetSlateWidget<UUserWidgetBase>(ParentName);
	}
	if(ParentWidget)
	{
		ParentWidget->AddChild(this);
	}
	//
	// for(const auto& Iter : FSlateWidgetManager::Get().GetUserWidgetChildrenByName(WidgetName))
	// {
	// 	if(FSlateWidgetManager::Get().HasUserWidgetClassByName(Iter))
	// 	{
	// 		const UUserWidgetBase* DefaultObject = FSlateWidgetManager::Get().GetUserWidgetClassByName(Iter)->GetDefaultObject<UUserWidgetBase>();
	// 		if(DefaultObject->ParentName == WidgetName && (DefaultObject->WidgetCreateType == EWidgetCreateType::AutoCreate || DefaultObject->WidgetCreateType == EWidgetCreateType::AutoCreateAndOpen))
	// 		{
	// 			FSlateWidgetManager::Get().CreateUserWidgetByName<UUserWidgetBase>(Iter, InOwner);
	// 		}
	// 	}
	// }
}

void SSlateWidgetBase::OnInitialize(UObject* InOwner, const TArray<FParameter>& InParams)
{
	OwnerObject = InOwner;
}

void SSlateWidgetBase::OnOpen(const TArray<FParameter>& InParams, bool bInstant)
{
	if(WidgetState == EScreenWidgetState::Opening || WidgetState == EScreenWidgetState::Opened) return;
	
	WidgetParams = InParams;
	WidgetState = EScreenWidgetState::Opening;
	OnStateChanged(WidgetState);

	if(!GetParentWidget())
	{
#if WITH_ENGINE
		GWorld->GetGameViewport()->AddViewportWidgetContent(SharedThis(this), WidgetZOrder);
#endif
	}

	switch(WidgetOpenType)
	{
		case EWidgetOpenType::Visible:
		{
			SetVisibility(EVisibility::Visible);
			break;
		}
		case EWidgetOpenType::HitTestInvisible:
		{
			SetVisibility(EVisibility::HitTestInvisible);
			break;
		}
		case EWidgetOpenType::SelfHitTestInvisible:
		{
			SetVisibility(EVisibility::SelfHitTestInvisible);
			break;
		}
		default: break;
	}
		
	Refresh();

	if(WidgetRefreshType == EWidgetRefreshType::Timer)
	{
		// UCommonStatics::GetCurrentTimerManager()->GetTimerManager().SetTimer(WidgetRefreshTimerHandle, this, &UUserWidgetBase::Refresh, WidgetRefreshTime, true);
	}
	if(bInstant)
	{
		FinishOpen(bInstant);
	}

	FInputManager::Get().UpdateInputMode();

	// if(K2_OnOpened.IsBound()) K2_OnOpened.Broadcast(InParams, bInstant);
	// if(OnOpened.IsBound()) OnOpened.Broadcast(InParams, bInstant);

	for(const auto Iter : ChildWidgets)
	{
		if(Iter->GetParentName() == GetWidgetName() && Iter->GetWidgetCreateType() == EWidgetCreateType::AutoCreateAndOpen)
		{
			Iter->Open(nullptr, bInstant);
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

void SSlateWidgetBase::OnDestroy(bool bRecovery)
{
#if WITH_ENGINE
	if(GWorld->GetGameViewport())
	{
		GWorld->GetGameViewport()->RemoveViewportWidgetContent(SharedThis(this));
	}
#endif

	if(ParentWidget)
	{
		ParentWidget->RemoveChild(this);
	}

	FInputManager::Get().UpdateInputMode();
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
	FSlateWidgetManager::Get().OpenSlateWidget<SSlateWidgetBase>(InParams, bInstant, GetWidgetName());
}

void SSlateWidgetBase::Open(const TArray<FParameter>& InParams, bool bInstant, bool bForce)
{
	FSlateWidgetManager::Get().OpenSlateWidget<SSlateWidgetBase>(InParams, bInstant, GetWidgetName());
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
	FSlateWidgetManager::Get().DestroySlateWidget<SSlateWidgetBase>(bRecovery, GetWidgetName());
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
#if WITH_ENGINE
			GWorld->GetGameViewport()->RemoveViewportWidgetContent(SharedThis(this));
#endif
		}
		default: break;
	}

	FInputManager::Get().UpdateInputMode();
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

TSharedPtr<SPanel> SSlateWidgetBase::GetRootPanelWidget() const
{
	return nullptr;
}

TSharedPtr<SPanel> SSlateWidgetBase::GetParentPanelWidget() const
{
	return nullptr;
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
