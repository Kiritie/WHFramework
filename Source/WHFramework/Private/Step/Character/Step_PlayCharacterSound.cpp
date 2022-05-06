// Fill out your copyright notice in the Description page of Project Settings.


#include "Step/Character/Step_PlayCharacterSound.h"

#include "Character/CharacterModuleBPLibrary.h"

UStep_PlayCharacterSound::UStep_PlayCharacterSound()
{
	StepName = FName("PlayCharacterSound");
	StepDisplayName = FText::FromString(TEXT("Character Sound"));

	StepType = EStepType::Standalone;

	CharacterName = NAME_None;
	Sound = nullptr;
}

void UStep_PlayCharacterSound::OnEnter(UStepBase* InLastStep)
{
	Super::OnEnter(InLastStep);
	
	UCharacterModuleBPLibrary::PlayCharacterSound(CharacterName, Sound, true);
}

void UStep_PlayCharacterSound::OnLeave()
{
	Super::OnLeave();
	
	UCharacterModuleBPLibrary::StopCharacterSound(CharacterName);
}
