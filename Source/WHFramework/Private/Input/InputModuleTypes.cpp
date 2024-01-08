// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/InputModuleTypes.h"

#include "Widgets/SViewport.h"

bool FInputKeyShortcut::IsPressed(APlayerController* InPlayerController, bool bAllowInvalid) const
{
	if(!IsValid() && bAllowInvalid) return true;
	bool ReturnValue = false;
	for(auto& Iter : Auxs)
	{
		if(InPlayerController->WasInputKeyJustPressed(Iter))
		{
			ReturnValue = true;
			break;
		}
	}
	for(auto& Iter : Auxs)
	{
		if(!InPlayerController->IsInputKeyDown(Iter))
		{
			ReturnValue = false;
			break;
		}
	}
	return ReturnValue;
}

bool FInputKeyShortcut::IsReleased(APlayerController* InPlayerController, bool bAllowInvalid) const
{
	if(!IsValid() && bAllowInvalid) return true;
	bool ReturnValue = false;
	for(auto& Iter : Auxs)
	{
		if(InPlayerController->WasInputKeyJustReleased(Iter))
		{
			ReturnValue = true;
			break;
		}
	}
	for(auto& Iter : Auxs)
	{
		if(!InPlayerController->IsInputKeyDown(Iter))
		{
			ReturnValue = false;
			break;
		}
	}
	return ReturnValue;
}

bool FInputKeyShortcut::IsPressing(APlayerController* InPlayerController, bool bAllowInvalid) const
{
	if(!IsValid() && bAllowInvalid) return true;
	bool ReturnValue = false;
	for(auto& Iter : Auxs)
	{
		if(InPlayerController->IsInputKeyDown(Iter))
		{
			ReturnValue = true;
			break;
		}
	}
	for(auto& Iter : Auxs)
	{
		if(!InPlayerController->IsInputKeyDown(Iter))
		{
			ReturnValue = false;
			break;
		}
	}
	return ReturnValue;
}

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
