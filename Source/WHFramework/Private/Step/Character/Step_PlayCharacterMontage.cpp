// Fill out your copyright notice in the Description page of Project Settings.


#include "Step/Character/Step_PlayCharacterMontage.h"

#include "Character/CharacterModuleBPLibrary.h"

UStep_PlayCharacterMontage::UStep_PlayCharacterMontage()
{
	StepName = FName("PlayCharacterMontage");
	StepDisplayName = FText::FromString(TEXT("Character Montage"));

	StepType = EStepType::Standalone;

	CharacterName = NAME_None;
	Montage = nullptr;
}

void UStep_PlayCharacterMontage::OnEnter(UStepBase* InLastStep)
{
	Super::OnEnter(InLastStep);
	
	UCharacterModuleBPLibrary::PlayCharacterMontage(CharacterName, Montage, true);
}

void UStep_PlayCharacterMontage::OnLeave()
{
	Super::OnLeave();
	
	UCharacterModuleBPLibrary::PlayCharacterMontage(CharacterName, Montage, true);
}
