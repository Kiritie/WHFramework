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
	return GetDeterminesOutputObject(GetInputSubsystem(InPlayerIndex)->GetUserSettings<UInputUserSettingsBase>(), InClass);
}

UInputComponentBase* UInputModuleStatics::GetInputComponent(int32 InPlayerIndex, TSubclassOf<UInputComponentBase> InClass)
{
	return GetDeterminesOutputObject(Cast<UInputComponentBase>(UCommonStatics::GetLocalPlayerController(InPlayerIndex)->InputComponent), InClass);
}

int32 UInputModuleStatics::GetTouchPressedCount()
{
	return UInputModule::Get().GetTouchPressedCount();
}

FInputKeyShortcut UInputModuleStatics::GetKeyShortcutByName(const FName InName)
{
	return UInputModule::Get().GetKeyShortcutByName(InName);
}

void UInputModuleStatics::AddKeyShortcut(const FName InName, const FInputKeyShortcut& InKeyShortcut)
{
	UInputModule::Get().AddKeyShortcut(InName, InKeyShortcut);
}

void UInputModuleStatics::RemoveKeyShortcut(const FName InName)
{
	UInputModule::Get().RemoveKeyShortcut(InName);
}

void UInputModuleStatics::AddKeyMapping(const FName InName, const FInputKeyMapping& InKeyMapping)
{
	UInputModule::Get().AddKeyMapping(InName, InKeyMapping);
}

void UInputModuleStatics::RemoveKeyMapping(const FName InName)
{
	UInputModule::Get().RemoveKeyMapping(InName);
}

void UInputModuleStatics::AddTouchMapping(const FInputTouchMapping& InKeyMapping)
{
	UInputModule::Get().AddTouchMapping(InKeyMapping);
}

const UInputActionBase* UInputModuleStatics::FindInputActionForTag(const FGameplayTag& InInputTag, bool bEnsured)
{
	return UInputModule::Get().FindInputActionForTag(InInputTag, bEnsured);
}

void UInputModuleStatics::AddOrUpdateCustomKeyBindings(const FName InName, const FKey InKey, int32 InSlot, int32 InPlayerIndex)
{
	UInputModule::Get().AddOrUpdateCustomKeyBindings(InName, InKey, InSlot, InPlayerIndex);
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
