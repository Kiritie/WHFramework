// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/InputModuleBPLibrary.h"

#include "Input/InputModule.h"
#include "Main/MainModule.h"
#include "Kismet/GameplayStatics.h"

EInputMode UInputModuleBPLibrary::GetGlobalInputMode()
{
	if(AInputModule* InputModule = AMainModule::GetModuleByClass<AInputModule>())
	{
		return InputModule->GetGlobalInputMode();
	}
	return EInputMode::None;
}

void UInputModuleBPLibrary::SetGlobalInputMode(EInputMode InInputMode)
{
	if(AInputModule* InputModule = AMainModule::GetModuleByClass<AInputModule>())
	{
		InputModule->SetGlobalInputMode(InInputMode);
	}
}

void UInputModuleBPLibrary::UpdateGlobalInputMode()
{
	if(AInputModule* InputModule = AMainModule::GetModuleByClass<AInputModule>())
	{
		InputModule->UpdateInputMode();
	}
}

bool UInputModuleBPLibrary::IsInputKeyDown(const UObject* InWorldContext, FKey InKey)
{
	if(AWHPlayerController* PlayerController = UGlobalBPLibrary::GetPlayerController<AWHPlayerController>(InWorldContext))
	{
		return PlayerController->IsInputKeyDown(InKey);
	}
	return false;
}
