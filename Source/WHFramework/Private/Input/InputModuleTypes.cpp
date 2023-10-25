// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/InputModuleTypes.h"

#include "Input/Base/InputActionBase.h"
#include "Widgets/SViewport.h"

bool FInputKeyShortcut::IsPressing(APlayerController* InPlayerController, bool bAllowInvalid) const
{
	if(!IsValid() && bAllowInvalid) return true;
	bool bPressing = InPlayerController->IsInputKeyDown(Key);
	for(auto& Iter : Auxs)
	{
		if(!InPlayerController->IsInputKeyDown(Iter))
		{
			bPressing = false;
			break;
		}
	}
	return bPressing;
}

void FInputModeNone::ApplyInputMode(class FReply& SlateOperations, UGameViewportClient& GameViewportClient) const
{
	TSharedPtr<SViewport> ViewportWidget = GameViewportClient.GetGameViewportWidget();
	if (ViewportWidget.IsValid())
	{
		TSharedRef<SViewport> ViewportWidgetRef = ViewportWidget.ToSharedRef();
		SlateOperations.UseHighPrecisionMouseMovement(ViewportWidgetRef);
		SlateOperations.SetUserFocus(ViewportWidgetRef);
		SlateOperations.LockMouseToWidget(ViewportWidgetRef);
		GameViewportClient.SetMouseLockMode(EMouseLockMode::LockAlways);
		GameViewportClient.SetIgnoreInput(true);
		GameViewportClient.SetMouseCaptureMode(EMouseCaptureMode::NoCapture);
	}
}

TArray<FEnhancedActionKeyMapping*> FInputModuleSaveData::GetActionMappingsByName(const FName InActionName)
{
	TArray<FEnhancedActionKeyMapping*> Mappings;
	for(auto& Iter1 : ActionMappings)
	{
		for(auto& Iter2 : Iter1.Mappings)
		{
			if(static_cast<UInputActionBase*>(Iter2.Action)->ActionName == InActionName)
			{
				Mappings.Add(&Iter2);
			}
		}
	}
	return Mappings;
}
