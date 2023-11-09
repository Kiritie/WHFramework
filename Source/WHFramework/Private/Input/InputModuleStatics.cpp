// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/InputModuleStatics.h"

#include "Common/CommonStatics.h"
#include "Input/InputModule.h"

int32 UInputModuleStatics::GetTouchPressedCount()
{
	if(UInputModule* InputModule = UInputModule::Get())
	{
		return InputModule->GetTouchPressedCount();
	}
	return 0;
}

FInputKeyShortcut UInputModuleStatics::GetKeyShortcutByName(const FName InName)
{
	if(UInputModule* InputModule = UInputModule::Get())
	{
		return InputModule->GetKeyShortcutByName(InName);
	}
	return FInputKeyShortcut();
}

EInputMode UInputModuleStatics::GetGlobalInputMode()
{
	if(UInputModule* InputModule = UInputModule::Get())
	{
		return InputModule->GetGlobalInputMode();
	}
	return EInputMode::None;
}

void UInputModuleStatics::SetGlobalInputMode(EInputMode InInputMode)
{
	if(UInputModule* InputModule = UInputModule::Get())
	{
		InputModule->SetGlobalInputMode(InInputMode);
	}
}

void UInputModuleStatics::UpdateGlobalInputMode()
{
	if(UInputModule* InputModule = UInputModule::Get())
	{
		InputModule->UpdateInputMode();
	}
}
