// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Scene/EventHandle_StopLevelSequence.h"

#include "Runtime/LevelSequence/Public/LevelSequenceActor.h"

UEventHandle_StopLevelSequence::UEventHandle_StopLevelSequence()
{
	LevelSequence = nullptr;
	Delay = 0.f;
	bKeepState = false;
}

void UEventHandle_StopLevelSequence::OnDespawn_Implementation(bool bRecovery)
{
	LevelSequence = nullptr;
	Delay = 0.f;
	bKeepState = false;
}

void UEventHandle_StopLevelSequence::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		LevelSequence = InParams[0].GetObjectPtrValue<ALevelSequenceActor>();
	}
	if(InParams.IsValidIndex(1))
	{
		Delay = InParams[1].GetFloatValue();
	}
	if(InParams.IsValidIndex(1))
	{
		bKeepState = InParams[2].GetBooleanValue();
	}
}

TArray<FParameter> UEventHandle_StopLevelSequence::Pack_Implementation()
{
	return { LevelSequence, Delay, bKeepState };
}
