// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/InputModuleStatics.h"

#include "Common/CommonStatics.h"
#include "Gameplay/WHLocalPlayer.h"
#include "Input/InputModule.h"
#include "EnhancedInputSubsystems.h"
#include "Input/InputManager.h"
#include "Input/Base/InputUserSettingsBase.h"
#include "Input/Components/InputComponentBase.h"

UEnhancedInputLocalPlayerSubsystem* UInputModuleStatics::GetInputSubsystem(int32 InPlayerIndex)
{
	if(UWHLocalPlayer* LocalPlayer = UCommonStatics::GetLocalPlayer(InPlayerIndex))
	{
		return LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	}
	return nullptr;
}

UInputUserSettingsBase* UInputModuleStatics::GetInputUserSettings(int32 InPlayerIndex, TSubclassOf<UWHLocalPlayer> InClass)
{
	if(UEnhancedInputLocalPlayerSubsystem* InputSubsystem = GetInputSubsystem(InPlayerIndex))
	{
		return GetDeterminesOutputObject(InputSubsystem->GetUserSettings<UInputUserSettingsBase>(), InClass);
	}
	return nullptr;
}

UInputComponentBase* UInputModuleStatics::GetInputComponent(int32 InPlayerIndex, TSubclassOf<UInputComponentBase> InClass)
{
	if(AWHPlayerController* PlayerController = UCommonStatics::GetLocalPlayerController(InPlayerIndex))
	{
		return GetDeterminesOutputObject(Cast<UInputComponentBase>(PlayerController->InputComponent), InClass);
	}
	return nullptr;
}

UInputManagerBase* UInputModuleStatics::GetInputManager(TSubclassOf<UInputManagerBase> InClass, int32 InPlayerIndex)
{
	return UInputModule::Get().GetInputManager(InClass, InPlayerIndex);
}

UInputManagerBase* UInputModuleStatics::GetInputManagerByName(const FName InName, int32 InPlayerIndex, TSubclassOf<UInputManagerBase> InClass)
{
	return UInputModule::Get().GetInputManagerByName(InName, InPlayerIndex, InClass);
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

FName UInputModuleStatics::GetPlayerKeyMappingName(const FGameplayTag& InTag)
{
	const FString TagName = InTag.ToString();
	return *TagName.Mid(TagName.Find(".", ESearchCase::IgnoreCase, ESearchDir::FromEnd) + 1, TagName.Len());
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

FPlayerKeyMappingInfo UInputModuleStatics::GetPlayerKeyMappingInfoByName(const FName InName, int32 InPlayerIndex)
{
	return UInputModule::Get().GetPlayerKeyMappingInfoByName(InName, InPlayerIndex);
}

bool UInputModuleStatics::IsPlayerMappedKeyByName(const FName InName, const FKey& InKey, int32 InPlayerIndex)
{
	return UInputModule::Get().IsPlayerMappedKeyByName(InName, InKey, InPlayerIndex);
}

bool UInputModuleStatics::IsPlayerMappedKeyByTag(const FGameplayTag& InTag, const FKey& InKey, int32 InPlayerIndex)
{
	return UInputModule::Get().IsPlayerMappedKeyByTag(InTag, InKey, InPlayerIndex);
}

const UInputActionBase* UInputModuleStatics::GetInputActionByTag(const FGameplayTag& InTag, bool bEnsured)
{
	return UInputModule::Get().GetInputActionByTag(InTag, bEnsured);
}

EInputMode UInputModuleStatics::GetGlobalInputMode()
{
	return FInputManager::Get().GetGlobalInputMode();
}

void UInputModuleStatics::UpdateGlobalInputMode()
{
	FInputManager::Get().UpdateInputMode();
}
