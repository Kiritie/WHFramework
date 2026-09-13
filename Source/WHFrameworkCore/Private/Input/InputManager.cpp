// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/InputManager.h"

#include "Kismet/GameplayStatics.h"
#include "Main/MainManager.h"

namespace
{
	int32 GetInputModeRank(EInputMode InInputMode)
	{
		switch(InInputMode)
		{
			case EInputMode::None:
				return 0;
			case EInputMode::GameOnly:
				return 10;
			case EInputMode::GameOnly_NotHideCursor:
				return 11;
			case EInputMode::GameAndUI:
				return 20;
			case EInputMode::GameAndUI_NotHideCursor:
				return 21;
			case EInputMode::UIOnly:
				return 30;
			default:
				return 0;
		}
	}
}

const FUniqueType FInputManager::Type = FUniqueType(&FManagerBase::Type);

IMPLEMENTATION_MANAGER(FInputManager)

// Sets default values
FInputManager::FInputManager() : FManagerBase(Type)
{
	NativeInputMode = EInputMode::GameOnly;
	
	GlobalInputMode = EInputMode::None;
	ExternalInputMode.Reset();
	bInputModeExternallyManaged = false;

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

void FInputManager::OnPreparatory()
{
	FManagerBase::OnPreparatory();

	UpdateInputMode();
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
	
	EInputMode InputMode = ExternalInputMode.Get(EInputMode::None);
	if(!ExternalInputMode.IsSet())
	{
		int32 InputPriority = MIN_int32;
		int32 InputModeRank = MIN_int32;
		for(IInputManagerInterface* InputManager : InputManagers)
		{
			if(!InputManager)
			{
				continue;
			}

			const EInputMode CandidateInputMode = InputManager->GetNativeInputMode();
			if(CandidateInputMode == EInputMode::None && InputManager != this)
			{
				continue;
			}

			const int32 CandidatePriority = InputManager->GetNativeInputPriority();
			const int32 CandidateModeRank = GetInputModeRank(CandidateInputMode);
			if(CandidatePriority > InputPriority
				|| (CandidatePriority == InputPriority && CandidateModeRank > InputModeRank))
			{
				InputPriority = CandidatePriority;
				InputModeRank = CandidateModeRank;
				InputMode = CandidateInputMode;
			}
		}
	}

	const bool bWasExternallyManaged = bInputModeExternallyManaged;
	const bool bIsExternallyManaged = ExternalInputMode.IsSet();
	if(GlobalInputMode != InputMode || bWasExternallyManaged != bIsExternallyManaged)
	{
		const EInputMode PreviousInputMode = GlobalInputMode;
		GlobalInputMode = InputMode;
		bInputModeExternallyManaged = bIsExternallyManaged;

		if(!ExternalInputMode.IsSet())
		{
			switch(InputMode)
			{
				case EInputMode::None:
					PlayerController->SetInputMode(FInputModeNone());
					PlayerController->bShowMouseCursor = false;
					break;
				case EInputMode::GameOnly:
					PlayerController->SetInputMode(FInputModeGameOnly());
					PlayerController->bShowMouseCursor = false;
					break;
				case EInputMode::GameOnly_NotHideCursor:
					PlayerController->SetInputMode(FInputModeGameOnly_NotHideCursor());
					PlayerController->bShowMouseCursor = true;
					break;
				case EInputMode::GameAndUI:
					PlayerController->SetInputMode(FInputModeGameAndUI());
					PlayerController->bShowMouseCursor = true;
					break;
				case EInputMode::GameAndUI_NotHideCursor:
					PlayerController->SetInputMode(FInputModeGameAndUI_NotHideCursor());
					PlayerController->bShowMouseCursor = true;
					break;
				case EInputMode::UIOnly:
					PlayerController->SetInputMode(FInputModeUIOnly());
					PlayerController->bShowMouseCursor = true;
					break;
				default:
					break;
			}
		}

		if(PreviousInputMode != GlobalInputMode)
		{
			OnInputModeChanged.Broadcast(PreviousInputMode, GlobalInputMode);
		}
	}
}

void FInputManager::SetExternalInputMode(TOptional<EInputMode> InInputMode)
{
	ExternalInputMode = InInputMode;
	UpdateInputMode();
}

void FInputManager::SetNativeInputMode(EInputMode InInputMode)
{
	NativeInputMode = InInputMode;
	UpdateInputMode();
}
