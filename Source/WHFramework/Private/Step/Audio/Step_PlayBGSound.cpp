// // Fill out your copyright notice in the Description page of Project Settings.
//
//
// #include "Step/Audio/Step_PlaySingleSound.h"
//
// #include "Engine/TargetPoint.h"
// #include "Audio/AudioModuleBPLibrary.h"
//
// UStep_PlaySingleSound::UStep_PlaySingleSound()
// {
// 	StepName = FName("PlaySingleSound");
// 	StepDisplayName = FText::FromString(TEXT("Single Sound"));
//
// 	StepType = EStepType::Standalone;
//
// 	Sound = nullptr;
// 	Volume = 1.f;
// 	Point = nullptr;
// }
//
// void UStep_PlaySingleSound::ServerOnEnter(UStepBase* InLastStep)
// {
// 	Super::ServerOnEnter(InLastStep);
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
// void UStep_PlaySingleSound::ServerOnLeave()
// {
// 	Super::ServerOnLeave();
// 	
// 	UAudioModuleBPLibrary::StopSingleSound(this);
// }
