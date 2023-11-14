// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/InputModuleStatics.h"

#include "Common/CommonStatics.h"
#include "Gameplay/WHLocalPlayer.h"
#include "Input/InputModule.h"
#include "EnhancedInputSubsystems.h"
#include "Input/Base/InputUserSettingsBase.h"
#include "Input/Components/InputComponentBase.h"

UEnhancedInputLocalPlayerSubsystem* UInputModuleStatics::GetInputSubsystem(int32 InPlayerIndex)
{
	return UCommonStatics::GetLocalPlayer(InPlayerIndex)->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
}

UInputUserSettingsBase* UInputModuleStatics::GetInputUserSettings(int32 InPlayerIndex, TSubclassOf<UWHLocalPlayer> InClass)
{
	return GetInputSubsystem(InPlayerIndex)->GetUserSettings<UInputUserSettingsBase>();
}

UInputComponentBase* UInputModuleStatics::GetInputComponent(int32 InPlayerIndex, TSubclassOf<UInputComponentBase> InClass)
{
	return Cast<UInputComponentBase>(UCommonStatics::GetLocalPlayerController(InPlayerIndex)->InputComponent);
}

int32 UInputModuleStatics::GetTouchPressedCount()
{
	return UInputModule::Get().GetTouchPressedCount();
}

FInputKeyShortcut UInputModuleStatics::GetKeyShortcutByName(const FName InName)
{
	return UInputModule::Get().GetKeyShortcutByName(InName);
}

EInputMode UInputModuleStatics::GetGlobalInputMode()
{
	return UInputModule::Get().GetGlobalInputMode();
}

void UInputModuleStatics::SetGlobalInputMode(EInputMode InInputMode)
{
	UInputModule::Get().SetGlobalInputMode(InInputMode);
}

void UInputModuleStatics::UpdateGlobalInputMode()
{
	UInputModule::Get().UpdateInputMode();
}
