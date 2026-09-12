// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/InputModuleStatics.h"

#include "Common/CommonModuleStatics.h"
#include "Gameplay/WHLocalPlayer.h"
#include "Input/InputModule.h"
#include "EnhancedInputSubsystems.h"
#include "Input/InputManager.h"
#include "Input/Base/InputUserSettingsBase.h"
#include "Input/Components/InputComponentBase.h"

UEnhancedInputLocalPlayerSubsystem* UInputModuleStatics::GetInputSubsystem(int32 InPlayerIndex)
{
	if(UWHLocalPlayer* LocalPlayer = UCommonModuleStatics::GetLocalPlayer(InPlayerIndex))
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
	if(AWHPlayerController* PlayerController = UCommonModuleStatics::GetLocalPlayerController(InPlayerIndex))
	{
		return GetDeterminesOutputObject(Cast<UInputComponentBase>(PlayerController->InputComponent), InClass);
	}
	return nullptr;
}

UInputBindingBase* UInputModuleStatics::GetInputBinding(TSubclassOf<UInputBindingBase> InClass, int32 InPlayerIndex)
{
	return UInputModule::Get().GetInputBinding(InClass, InPlayerIndex);
}

UInputBindingBase* UInputModuleStatics::GetInputBindingByName(const FName InName, int32 InPlayerIndex, TSubclassOf<UInputBindingBase> InClass)
{
	return UInputModule::Get().GetInputBindingByName(InName, InPlayerIndex, InClass);
}

EInputMode UInputModuleStatics::GetNativeInputMode()
{
	return UInputModule::Get().GetNativeInputMode();
}

void UInputModuleStatics::SetNativeInputMode(EInputMode InInputMode)
{
	UInputModule::Get().SetNativeInputMode(InInputMode);
}

TArray<FPlayerKeyMapping> UInputModuleStatics::GetAllPlayerKeyMappings(int32 InPlayerIndex)
{
	return UInputModule::Get().GetAllPlayerKeyMappings(InPlayerIndex);
}

bool UInputModuleStatics::IsPlayerMappedKeyByTag(const FGameplayTag& InTag, const FKey& InKey, int32 InPlayerIndex)
{
	return UInputModule::Get().IsPlayerMappedKeyByTag(InTag, InKey, InPlayerIndex);
}

const UInputActionBase* UInputModuleStatics::GetInputActionByTag(const FGameplayTag& InTag, bool bEnsured)
{
	return UInputModule::Get().GetInputActionByTag(InTag, bEnsured);
}

bool UInputModuleStatics::IsInputActionActive(const FGameplayTag& InTag, int32 InPlayerIndex)
{
	return UInputModule::Get().IsInputActionActive(InTag, InPlayerIndex);
}

bool UInputModuleStatics::ActivateInputContext(FGameplayTag InContextTag, int32 InPlayerIndex)
{
	return UInputModule::Get().ActivateInputContext(InContextTag, InPlayerIndex);
}

bool UInputModuleStatics::DeactivateInputContext(FGameplayTag InContextTag, int32 InPlayerIndex)
{
	return UInputModule::Get().DeactivateInputContext(InContextTag, InPlayerIndex);
}

bool UInputModuleStatics::IsInputContextActive(FGameplayTag InContextTag, int32 InPlayerIndex)
{
	return UInputModule::Get().IsInputContextActive(InContextTag, InPlayerIndex);
}

bool UInputModuleStatics::MapPlayerKeyByTag(FGameplayTag InActionTag, FKey InNewKey, EPlayerMappableKeySlot InSlot, FGameplayTagContainer& OutFailureReason, int32 InPlayerIndex)
{
	return UInputModule::Get().MapPlayerKeyByTag(InActionTag, InNewKey, InSlot, InPlayerIndex, &OutFailureReason);
}

bool UInputModuleStatics::ResetPlayerKeyByTag(FGameplayTag InActionTag, int32 InPlayerIndex)
{
	return UInputModule::Get().ResetPlayerKeyByTag(InActionTag, InPlayerIndex);
}

TArray<FPlayerKeyMapping> UInputModuleStatics::GetPlayerKeyMappingsByTag(FGameplayTag InActionTag, int32 InPlayerIndex)
{
	return UInputModule::Get().GetPlayerKeyMappingsByTag(InActionTag, InPlayerIndex);
}

FText UInputModuleStatics::GetPlayerKeyCodeByTag(FGameplayTag InActionTag, int32 InPlayerIndex)
{
	FString KeyCode;
	for(const FPlayerKeyMapping& Mapping : GetPlayerKeyMappingsByTag(InActionTag, InPlayerIndex))
	{
		if(!KeyCode.IsEmpty())
		{
			KeyCode += TEXT("/");
		}
		KeyCode += Mapping.GetCurrentKey().GetDisplayName(false).ToString();
	}
	return FText::FromString(KeyCode);
}

TArray<FGameplayTag> UInputModuleStatics::GetAllMappableActions()
{
	return UInputModule::Get().GetAllMappableActions();
}

ECommonInputType UInputModuleStatics::GetCurrentInputType(int32 InPlayerIndex)
{
	return UInputModule::Get().GetCurrentInputType(InPlayerIndex);
}

bool UInputModuleStatics::IsUsingGamepad(int32 InPlayerIndex)
{
	return UInputModule::Get().IsUsingGamepad(InPlayerIndex);
}

bool UInputModuleStatics::IsUsingMouseAndKeyboard(int32 InPlayerIndex)
{
	return UInputModule::Get().IsUsingMouseAndKeyboard(InPlayerIndex);
}

bool UInputModuleStatics::IsUsingTouch(int32 InPlayerIndex)
{
	return UInputModule::Get().IsUsingTouch(InPlayerIndex);
}

EInputMode UInputModuleStatics::GetGlobalInputMode()
{
	return FInputManager::Get().GetGlobalInputMode();
}

void UInputModuleStatics::UpdateGlobalInputMode()
{
	FInputManager::Get().UpdateInputMode();
}
