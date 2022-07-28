// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/InputModule.h"

#include "Main/Base/ModuleBase.h"
#include "Event/EventModuleBPLibrary.h"
#include "Event/Handle/Input/EventHandle_ChangeInputMode.h"
#include "Gameplay/WHPlayerController.h"
#include "Input/InputManager.h"
#include "Kismet/GameplayStatics.h"
#include "Global/GlobalBPLibrary.h"
#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"

// Sets default values
AInputModule::AInputModule()
{
	ModuleName = FName("InputModule");
}

#if WITH_EDITOR
void AInputModule::OnGenerate_Implementation()
{
	Super::OnGenerate_Implementation();
}

void AInputModule::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();
}
#endif

void AInputModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void AInputModule::OnPreparatory_Implementation()
{
	Super::OnPreparatory_Implementation();

	UpdateInputMode();
}

void AInputModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void AInputModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void AInputModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}

void AInputModule::UpdateInputMode()
{
	EInputMode TmpInputMode = EInputMode::None;
	for (auto Iter : AMainModule::GetAllModule())
	{
		if(IInputManager* InputManager = Cast<IInputManager>(Iter.GetObject()))
		{
			if ((int32)InputManager->GetNativeInputMode() > (int32)TmpInputMode)
			{
				TmpInputMode = InputManager->GetNativeInputMode();
			}
		}
	}
	SetGlobalInputMode(TmpInputMode);
}

void AInputModule::SetGlobalInputMode(EInputMode InInputMode)
{
	if(AWHPlayerController* PlayerController = UGlobalBPLibrary::GetPlayerController<AWHPlayerController>())
	{
		if(GlobalInputMode != InInputMode)
		{
			GlobalInputMode = InInputMode;
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
				case EInputMode::UIOnly:
				{
					PlayerController->SetInputMode(FInputModeUIOnly());
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
				default: break;
			}
			UEventModuleBPLibrary::BroadcastEvent(UEventHandle_ChangeInputMode::StaticClass(), EEventNetType::Multicast, this, { FParameter::MakePointer(&GlobalInputMode) });
		}
	}
}
