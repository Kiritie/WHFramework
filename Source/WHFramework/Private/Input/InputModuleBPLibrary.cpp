// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/InputModuleBPLibrary.h"

#include "Input/InputModule.h"
#include "Main/MainModule.h"
#include "Kismet/GameplayStatics.h"

EInputMode UInputModuleBPLibrary::GetInputMode()
{
	if(AInputModule* InputModule = AMainModule::GetModuleByClass<AInputModule>())
	{
		return InputModule->GetInputMode();
	}
	return EInputMode::None;
}

void UInputModuleBPLibrary::SetInputMode(EInputMode InInputMode)
{
	if(AInputModule* InputModule = AMainModule::GetModuleByClass<AInputModule>())
	{
		InputModule->SetInputMode(InInputMode);
	}
}

void UInputModuleBPLibrary::UpdateInputMode()
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
