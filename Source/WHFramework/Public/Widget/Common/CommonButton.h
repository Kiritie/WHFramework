// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonButtonBase.h"
#include "ObjectPool/ObjectPoolInterface.h"

#include "CommonButton.generated.h"

class UImage;
class UCommonTextBlockN;

class WHFRAMEWORK_API SCommonButtonN : public SButton
{
public:
	SLATE_BEGIN_ARGS(SCommonButtonN)
		: _Content()
		, _HAlign(HAlign_Fill)
		, _VAlign(VAlign_Fill)
		, _ClickMethod(EButtonClickMethod::DownAndUp)
		, _TouchMethod(EButtonTouchMethod::DownAndUp)
		, _PressMethod(EButtonPressMethod::DownAndUp)
		, _IsFocusable(true)
		, _IsInteractionEnabled(true)
	{}
	SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_STYLE_ARGUMENT(FButtonStyle, ButtonStyle)
		SLATE_ARGUMENT(EHorizontalAlignment, HAlign)
		SLATE_ARGUMENT(EVerticalAlignment, VAlign)
		SLATE_EVENT(FOnClicked, OnClicked)
		SLATE_EVENT(FOnClicked, OnDoubleClicked)
		SLATE_EVENT(FSimpleDelegate, OnPressed)
		SLATE_EVENT(FSimpleDelegate, OnReleased)
		SLATE_ARGUMENT(EButtonClickMethod::Type, ClickMethod)
		SLATE_ARGUMENT(EButtonTouchMethod::Type, TouchMethod)
		SLATE_ARGUMENT(EButtonPressMethod::Type, PressMethod)
		SLATE_ARGUMENT(bool, IsFocusable)
		SLATE_EVENT(FSimpleDelegate, OnReceivedFocus)
		SLATE_EVENT(FSimpleDelegate, OnLostFocus)

		/** Is interaction enabled? */
		SLATE_ARGUMENT(bool, IsButtonEnabled)
		SLATE_ARGUMENT(bool, IsInteractionEnabled)
		SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		OnDoubleClicked = InArgs._OnDoubleClicked;

		SButton::Construct(SButton::FArguments()
			.ButtonStyle(InArgs._ButtonStyle)
			.HAlign(InArgs._HAlign)
			.VAlign(InArgs._VAlign)
			.ClickMethod(InArgs._ClickMethod)
			.TouchMethod(InArgs._TouchMethod)
			.PressMethod(InArgs._PressMethod)
			.OnClicked(InArgs._OnClicked)
			.OnPressed(InArgs._OnPressed)
			.OnReleased(InArgs._OnReleased)
			.IsFocusable(InArgs._IsFocusable)
			.Content()
			[
				InArgs._Content.Widget
			]);

		SetCanTick(false);
		// Set the hover state to indicate that we want to override the default behavior
		SetHover(false);

		OnReceivedFocus = InArgs._OnReceivedFocus;
		OnLostFocus = InArgs._OnLostFocus;
		bIsButtonEnabled = InArgs._IsButtonEnabled;
		bIsInteractionEnabled = InArgs._IsInteractionEnabled;
		bHovered = false;
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;

	virtual void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

	virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override;

	virtual FReply OnTouchMoved(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent) override;

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	virtual FReply OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	void SetIsButtonEnabled(bool bInIsButtonEnabled);

	void SetIsButtonFocusable(bool bInIsButtonFocusable);

	void SetIsInteractionEnabled(bool bInIsInteractionEnabled);

	bool IsInteractable() const;

	/** Overridden to fire delegate for external listener */
	virtual FReply OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent);

	virtual void OnFocusLost(const FFocusEvent& InFocusEvent) override;

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

protected:
	/** Press the button */
	virtual void Press() override;

private:
	FOnClicked OnDoubleClicked;

	/** Delegate fired whenever focus is received */
	FSimpleDelegate OnReceivedFocus;

	/** Delegate fired whenever focus is lost */
	FSimpleDelegate OnLostFocus;

	/** True if the button is enabled */
	bool bIsButtonEnabled;

	/** True if clicking is enabled, to allow for things like double click */
	bool bIsInteractionEnabled;

	/** True if mouse over the widget */
	bool bHovered;
};

/** Custom UButton override that allows us to disable clicking without disabling the widget entirely */
UCLASS(Experimental)	// "Experimental" to hide it in the designer
class WHFRAMEWORK_API UCommonButtonInternalN : public UCommonButtonInternalBase
{
	GENERATED_BODY()

public:
	UCommonButtonInternalN(const FObjectInitializer& ObjectInitializer);

public:
	void SetButtonEnabled(bool bInIsButtonEnabled);
	void SetInteractionEnabled(bool bInIsInteractionEnabled);

	/** Updates the IsFocusable flag and updates the bIsFocusable flag of the underlying slate button widget */
	void SetButtonFocusable(bool bInIsButtonFocusable);
	bool IsHovered() const;
	bool IsPressed() const;

	void SetMinDesiredHeight(int32 InMinHeight);
	void SetMinDesiredWidth(int32 InMinWidth);

protected:
	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	// End of UWidget interface

	virtual FReply SlateHandleClickedOverride();
	virtual void SlateHandlePressedOverride();
	virtual void SlateHandleReleasedOverride();
	virtual FReply SlateHandleDoubleClicked();

	/** Called when internal slate button receives focus; Fires OnReceivedFocus */
	void SlateHandleOnReceivedFocus();

	/** Called when internal slate button loses focus; Fires OnLostFocus */
	void SlateHandleOnLostFocus();

	/** Cached pointer to the underlying slate button owned by this UWidget */
	TSharedPtr<class SCommonButtonN> MyCommonButtonN;
};

UCLASS(Abstract, BlueprintType, Blueprintable, ClassGroup = UI, meta = (Category = "Common UI", DisplayName = "Common Button", DisableNativeTick))
class WHFRAMEWORK_API UCommonButton : public UCommonButtonBase, public IObjectPoolInterface
{
	friend class UCommonButtonGroup;

	GENERATED_BODY()

public:
	UCommonButton(const FObjectInitializer& ObjectInitializer);
	
	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:
	virtual int32 GetLimit_Implementation() const override { return -1; }

	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;
		
	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	virtual void NativePreConstruct() override;

	virtual void NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	
	virtual void NativeOnCurrentTextStyleChanged() override;

	virtual void NativeOnClicked() override;

	virtual void NativeOnSelected(bool bBroadcast) override;

	virtual void NativeOnDeselected(bool bBroadcast) override;

public:
	virtual UCommonButtonInternalBase* ConstructInternalButton() override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	UImage* Img_Icon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	UCommonTextBlockN* Txt_Title;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Content")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ExposeOnSpawn = true, EditCondition = "bSelectable"), Category = "Selection")
	bool bStandalone;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ExposeOnSpawn))
	TArray<FParameter> WidgetParams;

public:
	UFUNCTION(BlueprintPure)
	FText GetTitle() const { return Title; }

	UFUNCTION(BlueprintCallable)
	void SetTitle(const FText InTitle);

	UFUNCTION(BlueprintPure)
	bool IsStandalone() const { return bStandalone; }

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = InBrush))
	void SetIconBrush(const FSlateBrush& InBrush);

	UFUNCTION(BlueprintCallable)
	void SetMinWidth(int32 InValue);

	UFUNCTION(BlueprintCallable)
	void SetMinHeight(int32 InValue);

	UFUNCTION(BlueprintPure)
	virtual TArray<FParameter> GetWidgetParams() const { return WidgetParams; }

	virtual TArray<FParameter> GetSpawnParams_Implementation() const override { return WidgetParams; }
};
