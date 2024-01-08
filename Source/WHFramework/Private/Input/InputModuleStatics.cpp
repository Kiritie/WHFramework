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

void UInputModuleStatics::AddKeyShortcut(const FGameplayTag& InTag, const FInputKeyShortcut& InKeyShortcut)
{
	UInputModule::Get().AddKeyShortcut(InTag, InKeyShortcut);
}

void UInputModuleStatics::RemoveKeyShortcut(const FGameplayTag& InTag)
{
	UInputModule::Get().RemoveKeyShortcut(InTag);
}

FInputKeyShortcut UInputModuleStatics::GetKeyShortcut(const FGameplayTag& InTag)
{
	return UInputModule::Get().GetKeyShortcut(InTag);
}

void UInputModuleStatics::AddKeyMapping(const FGameplayTag& InTag, const FInputKeyMapping& InKeyMapping)
{
	UInputModule::Get().AddKeyMapping(InTag, InKeyMapping);
}

void UInputModuleStatics::RemoveKeyMapping(const FGameplayTag& InTag)
{
	UInputModule::Get().RemoveKeyMapping(InTag);
}

void UInputModuleStatics::AddTouchMapping(const FInputTouchMapping& InTouchMapping)
{
	UInputModule::Get().AddTouchMapping(InTouchMapping);
}

void UInputModuleStatics::AddPlayerKeyMapping(const FName InName, const FKey InKey, int32 InSlot, int32 InPlayerIndex)
{
	UInputModule::Get().AddPlayerKeyMapping(InName, InKey, InSlot, InPlayerIndex);
}

TArray<FPlayerKeyMapping> UInputModuleStatics::GetAllPlayerKeyMappings(int32 InPlayerIndex)
{
	return UInputModule::Get().GetAllPlayerKeyMappings(InPlayerIndex);
}

TArray<FPlayerKeyMapping> UInputModuleStatics::GetPlayerKeyMappingsByName(const FName InName, int32 InPlayerIndex)
{
	return UInputModule::Get().GetPlayerKeyMappingsByName(InName, InPlayerIndex);
}

bool UInputModuleStatics::IsPlayerMappedKeyByName(const FName InName, const FKey& InKey, int32 InPlayerIndex)
{
	return UInputModule::Get().IsPlayerMappedKeyByName(InName, InKey, InPlayerIndex);
}

const UInputActionBase* UInputModuleStatics::GetInputActionByTag(const FGameplayTag& InTag, bool bEnsured)
{
	return UInputModule::Get().GetInputActionByTag(InTag, bEnsured);
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
