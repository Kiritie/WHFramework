// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Scene/EventHandle_PlayLevelSequence.h"

#include "Runtime/LevelSequence/Public/LevelSequenceActor.h"

UEventHandle_PlayLevelSequence::UEventHandle_PlayLevelSequence()
{
	LevelSequence = nullptr;
	Delay = 0.f;
	bReverse = false;
}

void UEventHandle_PlayLevelSequence::OnDespawn_Implementation(bool bRecovery)
{
	LevelSequence = nullptr;
	Delay = 0.f;
	bReverse = false;
}

void UEventHandle_PlayLevelSequence::Parse_Implementation(const TArray<FParameter>& InParams)
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
		bReverse = InParams[2].GetBooleanValue();
	}
}

TArray<FParameter> UEventHandle_PlayLevelSequence::Pack_Implementation()
{
	return { LevelSequence, Delay, bReverse };
}
