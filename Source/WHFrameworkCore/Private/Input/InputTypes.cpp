// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/InputTypes.h"
#include "Widgets/SViewport.h"

void FInputModeNone::ApplyInputMode(class FReply& SlateOperations, UGameViewportClient& GameViewportClient) const
{
	const TSharedPtr<SViewport> ViewportWidget = GameViewportClient.GetGameViewportWidget();
	if (ViewportWidget.IsValid())
	{
		const TSharedRef<SViewport> ViewportWidgetRef = ViewportWidget.ToSharedRef();
		SlateOperations.UseHighPrecisionMouseMovement(ViewportWidgetRef);
		SlateOperations.SetUserFocus(ViewportWidgetRef);
		SlateOperations.LockMouseToWidget(ViewportWidgetRef);
		GameViewportClient.SetMouseLockMode(EMouseLockMode::LockAlways);
		GameViewportClient.SetIgnoreInput(true);
		GameViewportClient.SetMouseCaptureMode(EMouseCaptureMode::NoCapture);
	}
}
