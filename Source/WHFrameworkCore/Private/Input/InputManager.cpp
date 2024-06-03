// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/InputManager.h"

#include "Kismet/GameplayStatics.h"
#include "Main/MainManager.h"

const FUniqueType FInputManager::Type = FUniqueType();

IMPLEMENTATION_MANAGER(FInputManager)

// Sets default values
FInputManager::FInputManager() : FManagerBase(Type)
{
	NativeInputMode = EInputMode::GameOnly;
	
	GlobalInputMode = EInputMode::None;

	InputManagers = TArray<IInputManagerInterface*>();
}

FInputManager::~FInputManager()
{
}

void FInputManager::OnInitialize()
{
	FManagerBase::OnInitialize();

	AddInputManager(this);
}

void FInputManager::OnReset()
{
	FManagerBase::OnReset();
	
	GlobalInputMode = EInputMode::GameOnly;
}

void FInputManager::OnRefresh(float DeltaSeconds)
{
	FManagerBase::OnRefresh(DeltaSeconds);
}

void FInputManager::OnTermination()
{
	FManagerBase::OnTermination();

	RemoveInputManager(this);
}

void FInputManager::AddInputManager(IInputManagerInterface* InInputManager)
{
	if(!InputManagers.Contains(InInputManager))
	{
		InputManagers.Add(InInputManager);
	}
}

void FInputManager::RemoveInputManager(IInputManagerInterface* InInputManager)
{
	if(InputManagers.Contains(InInputManager))
	{
		InputManagers.Remove(InInputManager);
	}
}

void FInputManager::UpdateInputMode()
{
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GWorldContext, 0);

	if(!PlayerController) return;
	
	EInputMode InputMode = EInputMode::None;
	for (auto Iter : InputManagers)
	{
		if ((int32)Iter->GetNativeInputMode() > (int32)InputMode)
		{
			InputMode = Iter->GetNativeInputMode();
		}
	}

	if(GlobalInputMode != InputMode)
	{
		GlobalInputMode = InputMode;

		switch (InputMode)
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
		// UEventModuleStatics::BroadcastEvent(UEventHandle_InputModeChanged::StaticClass(), this, { &GlobalInputMode }, EEventNetType::Multicast);
	}
}
