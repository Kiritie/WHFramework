// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/InputModuleStatics.h"

#include "Common/CommonStatics.h"
#include "Input/InputModule.h"

int32 UInputModuleStatics::GetTouchPressedCount()
{
	return UInputModule::Get().GetTouchPressedCount();
}

FInputKeyShortcut UInputModuleStatics::GetKeyShortcutByName(const FName InName)
{
	return UInputModule::Get().GetKeyShortcutByName(InName);
}

EInputMode UInputModuleStatics::GetGlobalInputMode()
{
	return UInputModule::Get().GetGlobalInputMode();
}

void UInputModuleStatics::SetGlobalInputMode(EInputMode InInputMode)
{
	UInputModule::Get().SetGlobalInputMode(InInputMode);
}

void UInputModuleStatics::UpdateGlobalInputMode()
{
	UInputModule::Get().UpdateInputMode();
}
