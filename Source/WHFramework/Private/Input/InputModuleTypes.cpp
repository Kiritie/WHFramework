// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/InputModuleTypes.h"

#include "Widgets/SViewport.h"

bool FInputKeyShortcut::IsPressed(APlayerController* InPlayerController, bool bAllowInvalid) const
{
	if(!IsValid() && bAllowInvalid) return true;
	bool ReturnValue = false;
	for(auto& Iter : Keys)
	{
		if(InPlayerController->WasInputKeyJustPressed(Iter))
		{
			ReturnValue = true;
			break;
		}
	}
	for(auto& Iter : Auxs)
	{
		if(!InPlayerController->IsInputKeyDown(Iter))
		{
			ReturnValue = false;
			break;
		}
	}
	return ReturnValue;
}

bool FInputKeyShortcut::IsReleased(APlayerController* InPlayerController, bool bAllowInvalid) const
{
	if(!IsValid() && bAllowInvalid) return true;
	bool ReturnValue = false;
	for(auto& Iter : Keys)
	{
		if(InPlayerController->WasInputKeyJustReleased(Iter))
		{
			ReturnValue = true;
			break;
		}
	}
	for(auto& Iter : Auxs)
	{
		if(!InPlayerController->IsInputKeyDown(Iter))
		{
			ReturnValue = false;
			break;
		}
	}
	return ReturnValue;
}

bool FInputKeyShortcut::IsPressing(APlayerController* InPlayerController, bool bAllowInvalid) const
{
	if(!IsValid() && bAllowInvalid) return true;
	bool ReturnValue = false;
	for(auto& Iter : Keys)
	{
		if(InPlayerController->IsInputKeyDown(Iter))
		{
			ReturnValue = true;
			break;
		}
	}
	for(auto& Iter : Auxs)
	{
		if(!InPlayerController->IsInputKeyDown(Iter))
		{
			ReturnValue = false;
			break;
		}
	}
	return ReturnValue;
}
