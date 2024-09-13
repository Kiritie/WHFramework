// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/Manager/InputManagerBase.h"

#include "Input/InputManager.h"
#include "Input/Components/InputComponentBase.h"

// ParamSets default values
UInputManagerBase::UInputManagerBase()
{
	InputManagerName = NAME_None;
}

void UInputManagerBase::OnInitialize()
{
	K2_OnInitialize();

	FInputManager::Get().AddInputManager(this);
}

void UInputManagerBase::OnReset()
{
}

void UInputManagerBase::OnRefresh(float DeltaSeconds)
{
	K2_OnRefresh(DeltaSeconds);
}

void UInputManagerBase::OnBindAction(UInputComponentBase* InInputComponent)
{
	K2_OnBindAction(InInputComponent);
}

void UInputManagerBase::OnTermination()
{
	K2_OnTermination();
	
	FInputManager::Get().RemoveInputManager(this);
}

EInputMode UInputManagerBase::GetNativeInputMode_Implementation() const
{
	return EInputMode::None;
}

int32 UInputManagerBase::GetNativeInputPriority_Implementation() const
{
	return 0;
}
