// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/InputModuleBPLibrary.h"

#include "Common/CommonBPLibrary.h"
#include "Input/InputModule.h"

int32 UInputModuleBPLibrary::GetTouchPressedCount()
{
	if(AInputModule* InputModule = AInputModule::Get())
	{
		return InputModule->GetTouchPressedCount();
	}
	return 0;
}

FInputKeyShortcut UInputModuleBPLibrary::GetKeyShortcutByName(const FName InName)
{
	if(AInputModule* InputModule = AInputModule::Get())
	{
		return InputModule->GetKeyShortcutByName(InName);
	}
	return FInputKeyShortcut();
}

EInputMode UInputModuleBPLibrary::GetGlobalInputMode()
{
	if(AInputModule* InputModule = AInputModule::Get())
	{
		return InputModule->GetGlobalInputMode();
	}
	return EInputMode::None;
}

void UInputModuleBPLibrary::SetGlobalInputMode(EInputMode InInputMode)
{
	if(AInputModule* InputModule = AInputModule::Get())
	{
		InputModule->SetGlobalInputMode(InInputMode);
	}
}

void UInputModuleBPLibrary::UpdateGlobalInputMode()
{
	if(AInputModule* InputModule = AInputModule::Get())
	{
		InputModule->UpdateInputMode();
	}
}
