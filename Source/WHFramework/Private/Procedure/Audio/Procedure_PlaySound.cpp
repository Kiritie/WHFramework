// // Fill out your copyright notice in the Description page of Project Settings.
//
//
// #include "Procedure/Audio/Procedure_PlaySingleSound.h"
//
// #include "Engine/TargetPoint.h"
// #include "Audio/AudioModuleBPLibrary.h"
//
// AProcedure_PlaySingleSound::AProcedure_PlaySingleSound()
// {
// 	ProcedureName = FName("PlaySingleSound");
// 	ProcedureDisplayName = FText::FromString(TEXT("Single Sound"));
//
// 	ProcedureType = EProcedureType::Standalone;
//
// 	Sound = nullptr;
// 	Volume = 1.f;
// 	Point = nullptr;
// }
//
// void AProcedure_PlaySingleSound::ServerOnEnter_Implementation(AProcedureBase* InLastProcedure)
// {
// 	Super::ServerOnEnter_Implementation(InLastProcedure);
// 	
// 	if(Point)
// 	{
// 		UAudioModuleBPLibrary::PlaySingleSoundAtLocation(this, Sound, Point->GetActorLocation(), Volume, true);
// 	}
// 	else
// 	{
// 		UAudioModuleBPLibrary::PlaySingleSound2D(this, Sound, Volume, true);
// 	}
// }
//
// void AProcedure_PlaySingleSound::ServerOnLeave_Implementation(AProcedureBase* InNextProcedure)
// {
// 	Super::ServerOnLeave_Implementation(InNextProcedure);
// 	
// 	UAudioModuleBPLibrary::StopSingleSound(this);
// }
