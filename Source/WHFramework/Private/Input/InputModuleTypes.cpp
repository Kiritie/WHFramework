// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/InputModuleTypes.h"

#include "Input/Base/InputActionBase.h"
#include "Widgets/SViewport.h"
#include "ICommonUIModule.h"
#include "CommonUISettings.h"

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

TArray<FEnhancedActionKeyMapping> FInputModuleSaveData::GetAllActionMappingByDisplayName(const FText InActionName)
{
	TArray<FEnhancedActionKeyMapping> Mappings;
	for(auto& Iter1 : ActionMappings)
	{
		if(Iter1.PlayerMappableOptions.DisplayName.EqualTo(InActionName))
		{
			Mappings.Add(Iter1);
		}
	}
	return Mappings;
}

bool FInputConfigMapping::CanBeActivated() const
{
	const FGameplayTagContainer& PlatformTraits = ICommonUIModule::GetSettings().GetPlatformTraits();

	// If the current platform does NOT have all the dependent traits, then don't activate it
	if (!DependentPlatformTraits.IsEmpty() && !PlatformTraits.HasAll(DependentPlatformTraits))
	{
		return false;
	}

	// If the platform has any of the excluded traits, then we shouldn't activate this config.
	if (!ExcludedPlatformTraits.IsEmpty() && PlatformTraits.HasAny(ExcludedPlatformTraits))
	{
		return false;
	}

	return true;
}
