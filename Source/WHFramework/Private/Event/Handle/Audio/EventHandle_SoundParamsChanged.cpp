// Fill out your copyright notice in the Description page of Project Settings.

#include "Event/Handle/Audio/EventHandle_SoundParamsChanged.h"

#include "Sound/SoundClass.h"

UEventHandle_SoundParamsChanged::UEventHandle_SoundParamsChanged()
{
	SoundClass = nullptr;
	SoundParams = FSoundParams();
}

void UEventHandle_SoundParamsChanged::OnDespawn_Implementation(bool bRecovery)
{
	SoundClass = nullptr;
	SoundParams = FSoundParams();
}

void UEventHandle_SoundParamsChanged::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		SoundClass = InParams[0].GetObjectValue<USoundClass>();
	}
	if(InParams.IsValidIndex(1))
	{
		SoundParams = InParams[1].GetPointerValueRef<FSoundParams>();
	}
}
