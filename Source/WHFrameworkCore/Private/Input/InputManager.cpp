// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/InputManager.h"

#include "Kismet/GameplayStatics.h"
#include "Main/MainManager.h"

const FUniqueType FInputManager::Type = FUniqueType();

IMPLEMENTATION_MANAGER(FInputManager)

// Sets default values
FInputManager::FInputManager()
{
	NativeInputMode = EInputMode::GameOnly;
	GlobalInputMode = EInputMode::GameOnly;
}

FInputManager::~FInputManager()
{
}

void FInputManager::UpdateInputMode()
{
	EInputMode InputMode = EInputMode::None;
	for (const auto Iter : FMainManager::GetAllManager<FInputManager>())
	{
		if ((int32)Iter->GetNativeInputMode() > (int32)InputMode)
		{
			InputMode = Iter->GetNativeInputMode();
		}
	}
	SetGlobalInputMode(InputMode);
}

bool FInputManager::SetGlobalInputMode(EInputMode InInputMode)
{
	if(GlobalInputMode != InInputMode)
	{
		GlobalInputMode = InInputMode;

		APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GWorld, 0);
		switch (InInputMode)
		{
			case EInputMode::None:
			{
				PlayerController->SetInputMode(FInputModeNone());
				PlayerController->bShowMouseCursor = false;
				break;
			}
			case EInputMode::GameOnly:
			{
				PlayerController->SetInputMode(FInputModeGameOnly());
				PlayerController->bShowMouseCursor = false;
				break;
			}
			case EInputMode::GameOnly_NotHideCursor:
			{
				PlayerController->SetInputMode(FInputModeGameOnly_NotHideCursor());
				PlayerController->bShowMouseCursor = true;
				break;
			}
			case EInputMode::GameAndUI:
			{
				PlayerController->SetInputMode(FInputModeGameAndUI());
				PlayerController->bShowMouseCursor = true;
				break;
			}
			case EInputMode::GameAndUI_NotHideCursor:
			{
				PlayerController->SetInputMode(FInputModeGameAndUI_NotHideCursor());
				PlayerController->bShowMouseCursor = true;
				break;
			}
			case EInputMode::UIOnly:
			{
				PlayerController->SetInputMode(FInputModeUIOnly());
				PlayerController->bShowMouseCursor = true;
				break;
			}
			default: break;
		}
		return true;
	}
	return false;
}
