// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/Audio/Procedure_PlaySingleSound.h"

#include "Engine/TargetPoint.h"
#include "Audio/AudioModuleBPLibrary.h"

UProcedure_PlaySingleSound::UProcedure_PlaySingleSound()
{
	ProcedureName = FName("PlaySingleSound");
	ProcedureDisplayName = FText::FromString(TEXT("Single Sound"));

	ProcedureType = EProcedureType::Standalone;

	Sound = nullptr;
	Volume = 1.f;
	Point = nullptr;
}

void UProcedure_PlaySingleSound::OnEnter_Implementation(UProcedureBase* InLastProcedure)
{
	Super::OnEnter_Implementation(InLastProcedure);
	
	if(Point)
	{
		UAudioModuleBPLibrary::PlaySingleSoundAtLocation(Sound, Point->GetActorLocation(), Volume, true);
	}
	else
	{
		UAudioModuleBPLibrary::PlaySingleSound2D(Sound, Volume, true);
	}
}

void UProcedure_PlaySingleSound::OnLeave_Implementation()
{
	Super::OnLeave_Implementation();
	
	UAudioModuleBPLibrary::StopSingleSound();
}
