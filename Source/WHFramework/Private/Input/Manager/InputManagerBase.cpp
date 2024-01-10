// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/Manager/InputManagerBase.h"

#include "Input/Components/InputComponentBase.h"

// ParamSets default values
UInputManagerBase::UInputManagerBase()
{
	InputManagerName = NAME_None;
}

void UInputManagerBase::OnInitialize()
{
	K2_OnInitialize();
}

void UInputManagerBase::OnBindAction(UInputComponentBase* InInputComponent)
{
	K2_OnBindAction(InInputComponent);
}
