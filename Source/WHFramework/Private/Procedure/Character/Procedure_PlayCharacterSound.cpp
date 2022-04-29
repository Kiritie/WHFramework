// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/Character/Procedure_PlayCharacterSound.h"

#include "Character/CharacterModuleBPLibrary.h"

UProcedure_PlayCharacterSound::UProcedure_PlayCharacterSound()
{
	ProcedureName = FName("PlayCharacterSound");
	ProcedureDisplayName = FText::FromString(TEXT("Character Sound"));

	ProcedureType = EProcedureType::Standalone;

	CharacterName = NAME_None;
	Sound = nullptr;
}

void UProcedure_PlayCharacterSound::OnEnter(UProcedureBase* InLastProcedure)
{
	Super::OnEnter(InLastProcedure);
	
	UCharacterModuleBPLibrary::PlayCharacterSound(CharacterName, Sound, true);
}

void UProcedure_PlayCharacterSound::OnLeave()
{
	Super::OnLeave();
	
	UCharacterModuleBPLibrary::StopCharacterSound(CharacterName);
}
