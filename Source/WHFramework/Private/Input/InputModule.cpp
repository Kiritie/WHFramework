// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/InputModule.h"

#include "Base/ModuleBase.h"
#include "Input/InputManager.h"
#include "Kismet/GameplayStatics.h"
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
	EInputMode TmpInputMode = EInputMode::GameOnly;
	for (auto Iter : AMainModule::GetAllModules())
	{
		if(IInputManager* InputManager = Cast<IInputManager>(Iter.GetObject()))
		{
			if(InputManager->GetNativeInputMode() != EInputMode::None)
			{
				if ((int32)InputManager->GetNativeInputMode() < (int32)TmpInputMode)
				{
					TmpInputMode = InputManager->GetNativeInputMode();
				}
			}
		}
	}
	SetInputMode(TmpInputMode);
}

void AInputModule::SetInputMode(EInputMode InInputMode)
{
	if(APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		if(InputMode != InInputMode)
		{
			InputMode = InInputMode;
			switch (InInputMode)
			{
				case EInputMode::None:
				{
					PlayerController->SetInputMode(FInputModeUIOnly());
					PlayerController->bShowMouseCursor = false;
					break;
				}
				case EInputMode::GameAndUI:
				{
					PlayerController->SetInputMode(FInputModeGameAndUI());
					PlayerController->bShowMouseCursor = true;
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
				default: break;
			}
			OnChangeInputMode.Broadcast(InputMode);
		}
	}
}

EInputMode AInputModule::GetNativeInputMode() const
{
	return EInputMode::None;
}
