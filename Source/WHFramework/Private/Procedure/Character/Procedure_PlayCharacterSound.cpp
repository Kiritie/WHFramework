// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/Character/Procedure_PlayCharacterSound.h"

#include "Character/CharacterModuleBPLibrary.h"

AProcedure_PlayCharacterSound::AProcedure_PlayCharacterSound()
{
	ProcedureName = FName("PlayCharacterSound");
	ProcedureDisplayName = FText::FromString(TEXT("Character Sound"));

	ProcedureType = EProcedureType::Standalone;

	CharacterName = NAME_None;
	Sound = nullptr;
}

void AProcedure_PlayCharacterSound::ServerOnEnter_Implementation(AProcedureBase* InLastProcedure)
{
	Super::ServerOnEnter_Implementation(InLastProcedure);
	
	UCharacterModuleBPLibrary::PlayCharacterSound(CharacterName, Sound, true);
}

void AProcedure_PlayCharacterSound::ServerOnLeave_Implementation(AProcedureBase* InNextProcedure)
{
	Super::ServerOnLeave_Implementation(InNextProcedure);
	
	UCharacterModuleBPLibrary::StopCharacterSound(CharacterName);
}
