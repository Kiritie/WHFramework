// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/Components/InputComponentBase.h"

UInputComponentBase::UInputComponentBase(const FObjectInitializer& ObjectInitializer)
{
}

void UInputComponentBase::RemoveBinds(TArray<uint32>& BindHandles)
{
	for (uint32 Handle : BindHandles)
	{
		RemoveBindingByHandle(Handle);
	}
	BindHandles.Reset();
}
