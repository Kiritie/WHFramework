// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/Character/Procedure_PlayCharacterMontage.h"

#include "Character/CharacterModuleBPLibrary.h"

UProcedure_PlayCharacterMontage::UProcedure_PlayCharacterMontage()
{
	ProcedureName = FName("PlayCharacterMontage");
	ProcedureDisplayName = FText::FromString(TEXT("Character Montage"));

	ProcedureType = EProcedureType::Standalone;

	CharacterName = TEXT("");
	Montage = nullptr;
}

void UProcedure_PlayCharacterMontage::OnEnter_Implementation(UProcedureBase* InLastProcedure)
{
	Super::OnEnter_Implementation(InLastProcedure);
	
	UCharacterModuleBPLibrary::PlayCharacterMontage(CharacterName, Montage, true);
}

void UProcedure_PlayCharacterMontage::OnLeave_Implementation()
{
	Super::OnLeave_Implementation();
	
	UCharacterModuleBPLibrary::PlayCharacterMontage(CharacterName, Montage, true);
}
