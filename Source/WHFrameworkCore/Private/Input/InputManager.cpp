// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/InputManager.h"

#include "Engine/Engine.h"
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
	DefaultInputMode = EInputMode::GameOnly;

	PlayerInputModes.Reset();
	InputModeRequests.Reset();
}

FInputManager::~FInputManager()
{
}

void FInputManager::OnInitialize()
{
	FManagerBase::OnInitialize();

}

void FInputManager::OnPreparatory()
{
	FManagerBase::OnPreparatory();

	UpdateInputMode();
}

void FInputManager::OnReset()
{
	FManagerBase::OnReset();

	PlayerInputModes.Reset();
	InputModeRequests.Reset();
	UpdateInputMode();
}

void FInputManager::OnRefresh(float DeltaSeconds)
{
	FManagerBase::OnRefresh(DeltaSeconds);
}

void FInputManager::OnTermination()
{
	FManagerBase::OnTermination();

}

void FInputManager::UpdateInputMode()
{
	TArray<TWeakObjectPtr<APlayerController>> Controllers;
	PlayerInputModes.GenerateKeyArray(Controllers);
	for (const TWeakObjectPtr<APlayerController>& Controller : Controllers)
	{
		APlayerController* PlayerController = Controller.Get();
		if (!PlayerController)
		{
			PlayerInputModes.Remove(Controller);
			continue;
		}
		FPlayerInputMode* CurrentMode = PlayerInputModes.Find(Controller);
		if (!CurrentMode) continue;
		FPlayerInputMode& PlayerMode = *CurrentMode;
		EInputMode InputMode = PlayerMode.CommonUIMode.Get(PlayerMode.DefaultMode);
		int32 InputPriority = MIN_int32;
		int32 InputModeRank = MIN_int32;
		for(const TPair<const void*, FInputModeRequest>& Pair : InputModeRequests)
		{
			const FInputModeRequest& Request = Pair.Value;
			const int32 CandidateModeRank = GetInputModeRank(Request.InputMode);
			if(Request.Priority > InputPriority
				|| (Request.Priority == InputPriority && CandidateModeRank > InputModeRank))
			{
				InputPriority = Request.Priority;
				InputModeRank = CandidateModeRank;
				InputMode = Request.InputMode;
			}
		}

		const bool bIsCommonUIControlled = PlayerMode.CommonUIMode.IsSet() && InputModeRequests.IsEmpty();
		if(!bIsCommonUIControlled)
		{
			PlayerController->ResetIgnoreMoveInput();
			PlayerController->ResetIgnoreLookInput();
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

		const EInputMode PreviousInputMode = PlayerMode.ResolvedMode;
		PlayerMode.ResolvedMode = InputMode;

		if(PreviousInputMode != InputMode)
		{
			OnInputModeChanged.Broadcast(PlayerController, PreviousInputMode, InputMode);
		}
	}
}

void FInputManager::RequestInputMode(const void* InOwner, EInputMode InInputMode, int32 InPriority)
{
	if(!InOwner) return;
	InputModeRequests.FindOrAdd(InOwner) = { InInputMode, InPriority };
	UpdateInputMode();
}

void FInputManager::ReleaseInputMode(const void* InOwner)
{
	if(!InOwner) return;
	InputModeRequests.Remove(InOwner);
	UpdateInputMode();
}

void FInputManager::SetDefaultInputMode(EInputMode InInputMode)
{
	DefaultInputMode = InInputMode;
	UpdateInputMode();
}

void FInputManager::SetPlayerInputMode(APlayerController* InController, EInputMode InDefaultInputMode, TOptional<EInputMode> InCommonUIInputMode)
{
	if (!InController) return;
	FPlayerInputMode& Mode = PlayerInputModes.FindOrAdd(InController);
	Mode.DefaultMode = InDefaultInputMode;
	Mode.CommonUIMode = InCommonUIInputMode;
	UpdateInputMode();
}

void FInputManager::ReleasePlayerInputMode(APlayerController* InController)
{
	if (InController) PlayerInputModes.Remove(InController);
}

EInputMode FInputManager::GetGlobalInputMode(APlayerController* InController) const
{
	const FPlayerInputMode* Mode = InController ? PlayerInputModes.Find(InController) : nullptr;
	return Mode ? Mode->ResolvedMode : DefaultInputMode;
}
