// Copyright Epic Games, Inc. All Rights Reserved.

#include "Setting/Widget/Misc/WidgetPressAnyKeyPanelBase.h"

#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"

class ICursor;

class FPressAnyKeyInputProcessor : public IInputProcessor
{
public:
	FPressAnyKeyInputProcessor()
	{

	}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override { }

	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		HandleKey(InKeyEvent.GetKey());
		return true;
	}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		return true;
	}

	virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		HandleKey(MouseEvent.GetEffectingButton());
		return true;
	}

	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		return true;
	}

	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		HandleKey(MouseEvent.GetEffectingButton());
		return true;
	}

	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& InWheelEvent, const FPointerEvent* InGestureEvent) override
	{
		if (InWheelEvent.GetWheelDelta() != 0)
		{
			const FKey Key = InWheelEvent.GetWheelDelta() < 0 ? EKeys::MouseScrollDown : EKeys::MouseScrollUp;
			HandleKey(Key);
		}
		return true;
	}

	DECLARE_MULTICAST_DELEGATE(FOnKeySelectionCanceled);
	FOnKeySelectionCanceled OnKeySelectionCanceled;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnKeySelected, FKey);
	FOnKeySelected OnKeySelected;

private:
	void HandleKey(const FKey& Key)
	{
		// Cancel this process if it's Escape, Touch, or a gamepad key.
		if (Key == EKeys::LeftCommand || Key == EKeys::RightCommand)
		{
			// Ignore
		}
		else if (Key == EKeys::Escape || Key.IsTouch() || Key.IsGamepadKey())
		{
			OnKeySelectionCanceled.Broadcast();
		}
		else
		{
			OnKeySelected.Broadcast(Key);
		}
	}
};

UWidgetPressAnyKeyPanelBase::UWidgetPressAnyKeyPanelBase(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	WidgetName = FName("PressAnyKeyPanel");

	WidgetType = EWidgetType::Permanent;
	WidgetInputMode = EInputMode::UIOnly;
}

void UWidgetPressAnyKeyPanelBase::OnInitialize(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnInitialize(InOwner, InParams);
}

void UWidgetPressAnyKeyPanelBase::OnOpen(const TArray<FParameter>& InParams, bool bInstant)
{
	Super::OnOpen(InParams, bInstant);

	bKeySelected = false;

	InputProcessor = MakeShared<FPressAnyKeyInputProcessor>();
	InputProcessor->OnKeySelected.AddUObject(this, &ThisClass::HandleKeySelected);
	InputProcessor->OnKeySelectionCanceled.AddUObject(this, &ThisClass::HandleKeySelectionCanceled);
	FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor, 0);
}

void UWidgetPressAnyKeyPanelBase::OnClose(bool bInstant)
{
	Super::OnClose(bInstant);

	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
	}
}

void UWidgetPressAnyKeyPanelBase::HandleKeySelected(FKey InKey)
{
	if (!bKeySelected)
	{
		bKeySelected = true;
		Dismiss([this, InKey]() {
			OnKeySelected.Broadcast(InKey);
		});
	}
}

void UWidgetPressAnyKeyPanelBase::HandleKeySelectionCanceled()
{
	if (!bKeySelected)
	{
		bKeySelected = true;
		Dismiss([this]() {
			OnKeySelectionCanceled.Broadcast();
		});
	}
}

void UWidgetPressAnyKeyPanelBase::Dismiss(TFunction<void()> PostDismissCallback)
{
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this, PostDismissCallback](float DeltaTime)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_UWidgetPressAnyKey_Dismiss);

		FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);

		Close();

		PostDismissCallback();

		return false;
	}));
}
