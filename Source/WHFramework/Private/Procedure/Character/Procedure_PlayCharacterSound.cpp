// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/Character/Procedure_PlayCharacterSound.h"

#include "Character/CharacterModuleBPLibrary.h"

UProcedure_PlayCharacterSound::UProcedure_PlayCharacterSound()
{
	ProcedureName = FName("PlayCharacterSound");
	ProcedureDisplayName = FText::FromString(TEXT("Character Sound"));

	ProcedureType = EProcedureType::Standalone;

	CharacterName = TEXT("");
	Sound = nullptr;
}

void UProcedure_PlayCharacterSound::OnEnter_Implementation(UProcedureBase* InLastProcedure)
{
	Super::OnEnter_Implementation(InLastProcedure);
	
	UCharacterModuleBPLibrary::PlayCharacterSound(CharacterName, Sound, true);
}

void UProcedure_PlayCharacterSound::OnLeave_Implementation()
{
	Super::OnLeave_Implementation();
	
	UCharacterModuleBPLibrary::StopCharacterSound(CharacterName);
}
