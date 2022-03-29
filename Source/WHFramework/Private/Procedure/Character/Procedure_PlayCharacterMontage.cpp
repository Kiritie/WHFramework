// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/Character/Procedure_PlayCharacterMontage.h"

#include "Character/CharacterModuleBPLibrary.h"

AProcedure_PlayCharacterMontage::AProcedure_PlayCharacterMontage()
{
	ProcedureName = FName("PlayCharacterMontage");
	ProcedureDisplayName = FText::FromString(TEXT("Character Montage"));

	ProcedureType = EProcedureType::Standalone;

	CharacterName = TEXT("");
	Montage = nullptr;
}

void AProcedure_PlayCharacterMontage::ServerOnEnter_Implementation(AProcedureBase* InLastProcedure)
{
	Super::ServerOnEnter_Implementation(InLastProcedure);
	
	UCharacterModuleBPLibrary::PlayCharacterMontage(CharacterName, Montage, true);
}

void AProcedure_PlayCharacterMontage::ServerOnLeave_Implementation(AProcedureBase* InNextProcedure)
{
	Super::ServerOnLeave_Implementation(InNextProcedure);
	
	UCharacterModuleBPLibrary::PlayCharacterMontage(CharacterName, Montage, true);
}
