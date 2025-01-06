// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widget/Common/CommonButton.h"

#include "Blueprint/WidgetTree.h"
#include "Components/ButtonSlot.h"
#include "Components/Image.h"
#include "Widget/Common/CommonTextBlockN.h"

FReply SCommonButtonN::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (IsInteractable())
	{
		FReply Reply = SButton::OnMouseButtonDown(MyGeometry, MouseEvent);
		if(Reply.GetMouseCaptor())
		{
			return Reply;
		}
	}
	return FReply::Unhandled();
}

FReply SCommonButtonN::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InPointerEvent)
{
	if (IsInteractable())
	{
		if (OnDoubleClicked.IsBound())
		{
			FReply Reply = OnDoubleClicked.Execute();
			if (Reply.IsEventHandled())
			{
				return Reply;
			}
		}

		// The default button behavior 'ignores' double click, which means you get,
		// down -> up, double-click -> up
		// which can make the input feel like it's being lost when players are clicking
		// a button over and over.  So if your button does not handle the double click
		// specifically, we'll treat the double click as a mouse up, and do whatever
		// we would normally do based on button configuration.
		if (!OnMouseButtonDown(InMyGeometry, InPointerEvent).IsEventHandled())
		{
			SButton::OnMouseButtonDoubleClick(InMyGeometry, InPointerEvent);
		}
	}

	return FReply::Unhandled();
}

FReply SCommonButtonN::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& InPointerEvent)
{
	if (IsInteractable())
	{
		FReply Reply = SButton::OnMouseButtonUp(MyGeometry, InPointerEvent);
		if(Reply.ShouldReleaseMouse())
		{
			return Reply;
		}
	}
	else if (HasMouseCapture())
	{
		// It's conceivable that interaction was disabled while this button had mouse capture
		// If that's the case, we want to release it (without acknowledging the click)
		Release();
		return FReply::Unhandled().ReleaseMouseCapture();
	}

	return FReply::Unhandled();
}

void SCommonButtonN::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& InPointerEvent)
{
	if (!InPointerEvent.IsTouchEvent())
	{
		const bool bWasHovered = IsHovered();

		bHovered = true;
		SetHover(bHovered && bIsInteractionEnabled);
		SButton::OnMouseEnter(MyGeometry, InPointerEvent);

		// SButton won't be able to correctly detect hover changes since we manually set hover, do our own detection
		if (!bWasHovered && IsHovered())
		{
			ExecuteHoverStateChanged(true);
		}
	}
}

void SCommonButtonN::OnMouseLeave(const FPointerEvent& InPointerEvent)
{
	if (InPointerEvent.IsTouchEvent())
	{
		if (HasMouseCapture())
		{
			Release();
		}
	}
	else
	{
		const bool bWasHovered = IsHovered();

		bHovered = false;
		SetHover(false);
		SButton::OnMouseLeave(InPointerEvent);

		// SButton won't be able to correctly detect hover changes since we manually set hover, do our own detection
		if (bWasHovered && !IsHovered())
		{
			ExecuteHoverStateChanged(true);
		}
	}
}

void SCommonButtonN::OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	if (!DragDropEvent.IsTouchEvent())
	{
		const bool bWasHovered = IsHovered();

		bHovered = true;
		SetHover(bHovered && bIsInteractionEnabled);
		SButton::OnDragEnter(MyGeometry, DragDropEvent);

		// SButton won't be able to correctly detect hover changes since we manually set hover, do our own detection
		if (!bWasHovered && IsHovered())
		{
			ExecuteHoverStateChanged(true);
		}
	}
}

void SCommonButtonN::OnDragLeave(const FDragDropEvent& DragDropEvent)
{
	if (DragDropEvent.IsTouchEvent())
	{
		if (HasMouseCapture())
		{
			Release();
		}
	}
	else
	{
		const bool bWasHovered = IsHovered();

		bHovered = false;
		SetHover(false);
		SButton::OnDragLeave(DragDropEvent);

		// SButton won't be able to correctly detect hover changes since we manually set hover, do our own detection
		if (bWasHovered && !IsHovered())
		{
			ExecuteHoverStateChanged(true);
		}
	}
}

FReply SCommonButtonN::OnTouchMoved(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent)
{
	if (HasMouseCapture())
	{
		if (!MyGeometry.IsUnderLocation(InTouchEvent.GetScreenSpacePosition()))
		{
			Release();
			return FReply::Unhandled().ReleaseMouseCapture();
		}
	}
	else
	{
		SButton::OnTouchMoved(MyGeometry, InTouchEvent);
	}

	return FReply::Unhandled();
}

FReply SCommonButtonN::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() != EKeys::Enter)
	{
		SButton::OnKeyDown(MyGeometry, InKeyEvent);
	}
	return FReply::Unhandled();
}

FReply SCommonButtonN::OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() != EKeys::Enter)
	{
		SButton::OnKeyUp(MyGeometry, InKeyEvent);
	}
	return FReply::Unhandled();
}

void SCommonButtonN::Press()
{
	if (IsInteractable())
	{
		SButton::Press();
	}
}

void SCommonButtonN::SetIsButtonEnabled(bool bInIsButtonEnabled)
{
	bIsButtonEnabled = bInIsButtonEnabled;
}

void SCommonButtonN::SetIsButtonFocusable(bool bInIsButtonFocusable)
{
	SetIsFocusable(bInIsButtonFocusable);
}

void SCommonButtonN::SetIsInteractionEnabled(bool bInIsInteractionEnabled)
{
	if (bIsInteractionEnabled == bInIsInteractionEnabled)
	{
		return;
	}

	const bool bWasHovered = IsHovered();

	bIsInteractionEnabled = bInIsInteractionEnabled;

	// If the hover state changed due to an intractability change, trigger external logic accordingly.
	const bool bIsHoveredNow = bHovered && bInIsInteractionEnabled;
	if (bWasHovered != bIsHoveredNow)
	{
		SetHover(bIsHoveredNow);
		ExecuteHoverStateChanged(false);
	}
}

bool SCommonButtonN::IsInteractable() const
{
	return bIsButtonEnabled && bIsInteractionEnabled;
}

/** Overridden to fire delegate for external listener */
FReply SCommonButtonN::OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent)
{
	FReply ReturnReply = SButton::OnFocusReceived(MyGeometry, InFocusEvent);
	OnReceivedFocus.ExecuteIfBound();

	return ReturnReply;
}

void SCommonButtonN::OnFocusLost(const FFocusEvent& InFocusEvent)
{
	OnLostFocus.ExecuteIfBound();
}

int32 SCommonButtonN::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	bool bEnabled = bParentEnabled && bIsButtonEnabled;
	return SButton::OnPaint(Args, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bEnabled);
}

//////////////////////////////////////////////////////////////////////////
// UCommonButtonInternalBaseN
//////////////////////////////////////////////////////////////////////////

static int32 bUseTransparentButtonStyleAsDefault = 0;
static FAutoConsoleVariableRef CVarUseTransparentButtonStyleAsDefault(
	TEXT("UseTransparentButtonStyleAsDefault"),
	bUseTransparentButtonStyleAsDefault,
	TEXT("If true, the default Button Style for the CommonButtonBase's SButton will be set to NoBorder, which has a transparent background and no padding"));

UCommonButtonInternalN::UCommonButtonInternalN(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
}

void UCommonButtonInternalN::SetButtonEnabled(bool bInIsButtonEnabled)
{
	bButtonEnabled = bInIsButtonEnabled;
	if (MyCommonButtonN.IsValid())
	{
		MyCommonButtonN->SetIsButtonEnabled(bInIsButtonEnabled);
	}
}

void UCommonButtonInternalN::SetButtonFocusable(bool bInIsButtonFocusable)
{
	InitIsFocusable(bInIsButtonFocusable);
	if (MyCommonButtonN.IsValid())
	{
		MyCommonButtonN->SetIsButtonFocusable(bInIsButtonFocusable);
	}
}

void UCommonButtonInternalN::SetInteractionEnabled(bool bInIsInteractionEnabled)
{
	if (bInteractionEnabled == bInIsInteractionEnabled)
	{
		return;
	}

	bInteractionEnabled = bInIsInteractionEnabled;
	if (MyCommonButtonN.IsValid())
	{
		MyCommonButtonN->SetIsInteractionEnabled(bInIsInteractionEnabled);
	}
}

bool UCommonButtonInternalN::IsHovered() const
{
	if (MyCommonButtonN.IsValid())
	{
		return MyCommonButtonN->IsHovered();
	}
	return false;
}

bool UCommonButtonInternalN::IsPressed() const
{
	if (MyCommonButtonN.IsValid())
	{
		return MyCommonButtonN->IsPressed();
	}
	return false;
}

void UCommonButtonInternalN::SetMinDesiredHeight(int32 InMinHeight)
{
	MinHeight = InMinHeight;
	if (MyBox.IsValid())
	{
		MyBox->SetMinDesiredHeight(InMinHeight);
	}
}

void UCommonButtonInternalN::SetMinDesiredWidth(int32 InMinWidth)
{
	MinWidth = InMinWidth;
	if (MyBox.IsValid())
	{
		MyBox->SetMinDesiredWidth(InMinWidth);
	}
}

TSharedRef<SWidget> UCommonButtonInternalN::RebuildWidget()
{
	MyButton = MyCommonButtonN = SNew(SCommonButtonN)
		.OnClicked(BIND_UOBJECT_DELEGATE(FOnClicked, SlateHandleClickedOverride))
		.OnDoubleClicked(BIND_UOBJECT_DELEGATE(FOnClicked, SlateHandleDoubleClicked))
		.OnPressed(BIND_UOBJECT_DELEGATE(FSimpleDelegate, SlateHandlePressedOverride))
		.OnReleased(BIND_UOBJECT_DELEGATE(FSimpleDelegate, SlateHandleReleasedOverride))
		.ButtonStyle(&GetStyle())
		.ClickMethod(GetClickMethod())
		.TouchMethod(GetTouchMethod())
		.IsFocusable(GetIsFocusable())
		.IsButtonEnabled(bButtonEnabled)
		.IsInteractionEnabled(bInteractionEnabled)
		.OnReceivedFocus(BIND_UOBJECT_DELEGATE(FSimpleDelegate, SlateHandleOnReceivedFocus))
		.OnLostFocus(BIND_UOBJECT_DELEGATE(FSimpleDelegate, SlateHandleOnLostFocus));

	MyBox = SNew(SBox)
		.MinDesiredWidth(MinWidth)
		.MinDesiredHeight(MinHeight)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			MyCommonButtonN.ToSharedRef()
		];

	if (GetChildrenCount() > 0)
	{
		Cast<UButtonSlot>(GetContentSlot())->BuildSlot(MyCommonButtonN.ToSharedRef());
	}

	return MyBox.ToSharedRef();
}

void UCommonButtonInternalN::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MyCommonButtonN.Reset();
	MyBox.Reset();
}

FReply UCommonButtonInternalN::SlateHandleClickedOverride()
{
	return SlateHandleClicked();
}

void UCommonButtonInternalN::SlateHandlePressedOverride()
{
	SlateHandlePressed();
}

void UCommonButtonInternalN::SlateHandleReleasedOverride()
{
	SlateHandleReleased();
}

FReply UCommonButtonInternalN::SlateHandleDoubleClicked()
{
	FReply Reply = FReply::Unhandled();
	if (HandleDoubleClicked.IsBound())
	{
		Reply = HandleDoubleClicked.Execute();
	}

	if (OnDoubleClicked.IsBound())
	{
		OnDoubleClicked.Broadcast();
		Reply = FReply::Handled();
	}

	return Reply;
}

void UCommonButtonInternalN::SlateHandleOnReceivedFocus()
{
	OnReceivedFocus.ExecuteIfBound();
}

void UCommonButtonInternalN::SlateHandleOnLostFocus()
{
	OnLostFocus.ExecuteIfBound();
}

UCommonButton::UCommonButton(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Img_Icon = nullptr;
	Txt_Title = nullptr;

	bStandalone = false;

	static ConstructorHelpers::FClassFinder<UCommonButtonStyle> StyleClassFinder(TEXT("/Script/Engine.Blueprint'/WHFramework/Widget/Blueprints/Common/_Style/Button/CBS_Default.CBS_Default_C'"));
	if(StyleClassFinder.Succeeded())
	{
		Style = StyleClassFinder.Class;
	}
}

void UCommonButton::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	
}

void UCommonButton::OnDespawn_Implementation(bool bRecovery)
{
	SetTitle(FText::GetEmpty());

	OnSelectedChangedBase.Clear();
	OnButtonBaseClicked.Clear();
	OnButtonBaseDoubleClicked.Clear();
	OnButtonBaseHovered.Clear();
	OnButtonBaseUnhovered.Clear();

	if(GetSelected())
	{
		SetSelectedInternal(false, false, false);
	}

	HoldReset();

	RemoveFromParent();
}

void UCommonButton::NativePreConstruct()
{
	Super::NativePreConstruct();

	SetTitle(Title);
}

void UCommonButton::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);

	if (!InDragDropEvent.IsTouchEvent())
	{
		if (GetIsEnabled() && IsInteractionEnabled())
		{
			NativeOnHovered();
		}
	}
}

void UCommonButton::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);

	if (!InDragDropEvent.IsTouchEvent())
	{
		if (GetIsEnabled() && IsInteractionEnabled())
		{
			NativeOnUnhovered();
		}
	}
}

void UCommonButton::NativeOnCurrentTextStyleChanged()
{
	Super::NativeOnCurrentTextStyleChanged();

	if(Txt_Title)
	{
		Txt_Title->SetStyle(GetCurrentTextStyleClass());
	}
}

void UCommonButton::NativeOnClicked()
{
	Super::NativeOnClicked();
}

void UCommonButton::NativeOnSelected(bool bBroadcast)
{
	Super::NativeOnSelected(bBroadcast);
}

void UCommonButton::NativeOnDeselected(bool bBroadcast)
{
	Super::NativeOnDeselected(bBroadcast);
}

UCommonButtonInternalBase* UCommonButton::ConstructInternalButton()
{
	return WidgetTree->ConstructWidget<UCommonButtonInternalN>(UCommonButtonInternalN::StaticClass(), FName(TEXT("InternalRootButtonBase")));
}

void UCommonButton::SetTitle(const FText InTitle)
{
	Title = InTitle;
	if(Txt_Title)
	{
		Txt_Title->SetText(Title);
		Txt_Title->SetVisibility(Title.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
}

void UCommonButton::SetIconBrush(const FSlateBrush& InBrush)
{
	if(Img_Icon)
	{
		Img_Icon->SetBrush(InBrush);
		Img_Icon->SetVisibility(!InBrush.HasUObject() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
}

void UCommonButton::SetMinWidth(int32 InValue)
{
	MinWidth = InValue;
}

void UCommonButton::SetMinHeight(int32 InValue)
{
	MinHeight = InValue;
}
