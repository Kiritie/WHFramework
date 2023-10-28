// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/InputModuleTypes.h"

#include "Input/InputModule.h"
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
			// if(static_cast<UInputActionBase*>(Iter2.Action)->ActionTag == InActionName)
			// {
			// 	Mappings.Add(&Iter2);
			// }
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

bool FInputConfigMapping::RegisterPair(const FInputConfigMapping& Pair)
{
	if (AInputModule* Settings = AInputModule::Get())
	{
		// Register the pair with the settings, but do not activate it yet
		if (const UPlayerMappableInputConfig* LoadedConfig = Pair.Config.LoadSynchronous())
		{
			Settings->RegisterInputConfig(Pair.Type, LoadedConfig, false);
			return true;
		}	
	}
	
	return false;
}

void FInputConfigMapping::UnregisterPair(const FInputConfigMapping& Pair)
{
	if (AInputModule* Settings = AInputModule::Get())
	{
		if (const UPlayerMappableInputConfig* LoadedConfig = Pair.Config.LoadSynchronous())
		{
			Settings->UnregisterInputConfig(LoadedConfig);
		}
	}
}

