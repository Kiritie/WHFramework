// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/Manager/InputBindingBase.h"

#include "Input/Components/InputComponentBase.h"

// ParamSets default values
UInputBindingBase::UInputBindingBase()
{
	InputBindingName = NAME_None;
	LocalPlayerIndex = INDEX_NONE;
	BoundInputComponent = nullptr;
}

void UInputBindingBase::OnInitialize(int32 InPlayerIndex)
{
	LocalPlayerIndex = InPlayerIndex;
	K2_OnInitialize();
}

void UInputBindingBase::OnReset()
{
	K2_OnReset();
}

void UInputBindingBase::OnRefresh(float DeltaSeconds)
{
	K2_OnRefresh(DeltaSeconds);
}

void UInputBindingBase::OnBindInput(UInputComponentBase* InInputComponent)
{
	BoundInputComponent = InInputComponent;
	K2_OnBindInput(InInputComponent);
}

void UInputBindingBase::OnUnbindInput()
{
	if(BoundInputComponent)
	{
		BoundInputComponent->ClearBindingsForObject(this);
		BoundInputComponent->RemoveBinds(BindingHandles);
	}
	BoundInputComponent = nullptr;
}

void UInputBindingBase::OnTermination()
{
	OnUnbindInput();
	K2_OnTermination();
	LocalPlayerIndex = INDEX_NONE;
}
