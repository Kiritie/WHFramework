// Fill out your copyright notice in the Description page of Project Settings.


#include "Audio/AudioModuleStatics.h"
#include "Common/LanguageTypes.h"

#include "Audio/AudioModule.h"
#include "Engine/World.h"

TArray<FLanguageType> UAudioModuleStatics::GetAudioLanguageTypes(const UObject* WorldContextObject)
{
	return UAudioModule::Get(WorldContextObject ? WorldContextObject->GetWorld() : nullptr).GetLanguageTypes();
}

void UAudioModuleStatics::SetAudioLanguageTypes(const UObject* WorldContextObject, const TArray<FLanguageType>& InLanguageTypes)
{
	UAudioModule::Get(WorldContextObject ? WorldContextObject->GetWorld() : nullptr).SetLanguageTypes(InLanguageTypes);
}

int32 UAudioModuleStatics::GetAudioLanguageType(const UObject* WorldContextObject)
{
	return UAudioModule::Get(WorldContextObject ? WorldContextObject->GetWorld() : nullptr).GetLanguageType();
}

void UAudioModuleStatics::SetAudioLanguageType(const UObject* WorldContextObject, int32 InLanguageType)
{
	UAudioModule::Get(WorldContextObject ? WorldContextObject->GetWorld() : nullptr).SetLanguageType(InLanguageType);
}

FNetworkDispatchResult UAudioModuleStatics::PlaySound2D(const UObject* WorldContextObject, USoundBase* InSound, float InVolume, bool bMulticast, APlayerController* InController)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!::IsValid(World) || World->bIsTearingDown) return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::InvalidContext);
	UAudioModule* Audio = UAudioModule::GetPtr(World);
	return Audio ? Audio->PlaySound2D(InSound, InVolume, bMulticast, InController) : FNetworkDispatchResult::Failure(World ? ENetworkDispatchStatus::ModuleUnavailable : ENetworkDispatchStatus::InvalidContext);
}

FNetworkDispatchResult UAudioModuleStatics::PlaySoundAtLocation(const UObject* WorldContextObject, USoundBase* InSound, FVector InLocation, float InVolume, bool bMulticast, APlayerController* InController)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!::IsValid(World) || World->bIsTearingDown) return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::InvalidContext);
	UAudioModule* Audio = UAudioModule::GetPtr(World);
	return Audio ? Audio->PlaySoundAtLocation(InSound, InLocation, InVolume, bMulticast, InController) : FNetworkDispatchResult::Failure(World ? ENetworkDispatchStatus::ModuleUnavailable : ENetworkDispatchStatus::InvalidContext);
}

FSingleSoundInfo UAudioModuleStatics::GetSingleSoundInfo(const UObject* WorldContextObject, const FSingleSoundHandle& InHandle, APlayerController* InController)
{
	const UAudioModule* Audio = UAudioModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Audio ? Audio->GetSingleSoundInfo(InHandle, InController) : FSingleSoundInfo();
}

EAudioNetworkSoundState UAudioModuleStatics::GetSingleSoundRequestState(const UObject* WorldContextObject, const FSingleSoundHandle& InHandle, APlayerController* InController)
{
	UAudioModule* Audio = UAudioModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Audio ? Audio->GetSingleSoundRequestState(InHandle, InController) : EAudioNetworkSoundState::Unknown;
}

FNetworkDispatchResult UAudioModuleStatics::GetSingleSoundDispatchResult(const UObject* WorldContextObject, const FSingleSoundHandle& InHandle, APlayerController* InController)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!::IsValid(World) || World->bIsTearingDown) return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::InvalidContext);
	UAudioModule* Audio = UAudioModule::GetPtr(World);
	return Audio ? Audio->GetSingleSoundDispatchResult(InHandle, InController) : FNetworkDispatchResult::Failure(World ? ENetworkDispatchStatus::ModuleUnavailable : ENetworkDispatchStatus::InvalidContext);
}

FSoundPlaybackResult UAudioModuleStatics::PlaySingleSound2D(const UObject* WorldContextObject, USoundBase* InSound, float InVolume, bool bMulticast, APlayerController* InController)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!::IsValid(World) || World->bIsTearingDown) { FSoundPlaybackResult Result; Result.Dispatch.Status = ENetworkDispatchStatus::InvalidContext; return Result; }
	UAudioModule* Audio = UAudioModule::GetPtr(World);
	if (Audio) return Audio->PlaySingleSound2D(InSound, InVolume, bMulticast, InController);
	FSoundPlaybackResult Result;
	Result.Dispatch.Status = World ? ENetworkDispatchStatus::ModuleUnavailable : ENetworkDispatchStatus::InvalidContext;
	return Result;
}

FSoundPlaybackResult UAudioModuleStatics::PlaySingleSound2DWithDelegate(const UObject* WorldContextObject, USoundBase* InSound, const FOnSoundPlayFinishDelegate& InOnSoundPlayFinish, float InVolume, bool bMulticast, APlayerController* InController)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!::IsValid(World) || World->bIsTearingDown) { FSoundPlaybackResult Result; Result.Dispatch.Status = ENetworkDispatchStatus::InvalidContext; return Result; }
	UAudioModule* Audio = UAudioModule::GetPtr(World);
	if (Audio) return Audio->PlaySingleSound2DWithDelegate(InSound, InOnSoundPlayFinish, InVolume, bMulticast, InController);
	FSoundPlaybackResult Result;
	Result.Dispatch.Status = World ? ENetworkDispatchStatus::ModuleUnavailable : ENetworkDispatchStatus::InvalidContext;
	return Result;
}

FSoundPlaybackResult UAudioModuleStatics::PlaySingleSoundAtLocation(const UObject* WorldContextObject, USoundBase* InSound, FVector InLocation, float InVolume, bool bMulticast, APlayerController* InController)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!::IsValid(World) || World->bIsTearingDown) { FSoundPlaybackResult Result; Result.Dispatch.Status = ENetworkDispatchStatus::InvalidContext; return Result; }
	UAudioModule* Audio = UAudioModule::GetPtr(World);
	if (Audio) return Audio->PlaySingleSoundAtLocation(InSound, InLocation, InVolume, bMulticast, InController);
	FSoundPlaybackResult Result;
	Result.Dispatch.Status = World ? ENetworkDispatchStatus::ModuleUnavailable : ENetworkDispatchStatus::InvalidContext;
	return Result;
}

FSoundPlaybackResult UAudioModuleStatics::PlaySingleSoundAtLocationWithDelegate(const UObject* WorldContextObject, USoundBase* InSound, const FOnSoundPlayFinishDelegate& InOnSoundPlayFinish, FVector InLocation, float InVolume, bool bMulticast, APlayerController* InController)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!::IsValid(World) || World->bIsTearingDown) { FSoundPlaybackResult Result; Result.Dispatch.Status = ENetworkDispatchStatus::InvalidContext; return Result; }
	UAudioModule* Audio = UAudioModule::GetPtr(World);
	if (Audio) return Audio->PlaySingleSoundAtLocationWithDelegate(InSound, InOnSoundPlayFinish, InLocation, InVolume, bMulticast, InController);
	FSoundPlaybackResult Result;
	Result.Dispatch.Status = World ? ENetworkDispatchStatus::ModuleUnavailable : ENetworkDispatchStatus::InvalidContext;
	return Result;
}

FNetworkDispatchResult UAudioModuleStatics::StopSingleSound(const UObject* WorldContextObject, const FSingleSoundHandle& InHandle, bool bMulticast, APlayerController* InController)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!::IsValid(World) || World->bIsTearingDown) return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::InvalidContext);
	UAudioModule* Audio = UAudioModule::GetPtr(World);
	return Audio ? Audio->StopSingleSound(InHandle, bMulticast, InController) : FNetworkDispatchResult::Failure(World ? ENetworkDispatchStatus::ModuleUnavailable : ENetworkDispatchStatus::InvalidContext);
}

FNetworkDispatchResult UAudioModuleStatics::SetSingleSoundPaused(const UObject* WorldContextObject, const FSingleSoundHandle& InHandle, bool bPaused, bool bMulticast, APlayerController* InController)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!::IsValid(World) || World->bIsTearingDown) return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::InvalidContext);
	UAudioModule* Audio = UAudioModule::GetPtr(World);
	return Audio ? Audio->SetSingleSoundPaused(InHandle, bPaused, bMulticast, InController) : FNetworkDispatchResult::Failure(World ? ENetworkDispatchStatus::ModuleUnavailable : ENetworkDispatchStatus::InvalidContext);
}

float UAudioModuleStatics::GetGlobalSoundVolume(const UObject* WorldContextObject)
{
	return UAudioModule::Get(WorldContextObject ? WorldContextObject->GetWorld() : nullptr).GetGlobalSoundVolume();
}

void UAudioModuleStatics::SetGlobalSoundVolume(const UObject* WorldContextObject, float InVolume, float InFadeInTime)
{
	UAudioModule::Get(WorldContextObject ? WorldContextObject->GetWorld() : nullptr).SetGlobalSoundVolume(InVolume, InFadeInTime);
}

float UAudioModuleStatics::GetGlobalSoundPitch(const UObject* WorldContextObject)
{
	return UAudioModule::Get(WorldContextObject ? WorldContextObject->GetWorld() : nullptr).GetGlobalSoundPitch();
}

void UAudioModuleStatics::SetGlobalSoundPitch(const UObject* WorldContextObject, float InPitch, float InFadeInTime)
{
	UAudioModule::Get(WorldContextObject ? WorldContextObject->GetWorld() : nullptr).SetGlobalSoundPitch(InPitch, InFadeInTime);
}

float UAudioModuleStatics::GetBackgroundSoundVolume(const UObject* WorldContextObject)
{
	return UAudioModule::Get(WorldContextObject ? WorldContextObject->GetWorld() : nullptr).GetBackgroundSoundVolume();
}

void UAudioModuleStatics::SetBackgroundSoundVolume(const UObject* WorldContextObject, float InVolume, float InFadeInTime)
{
	UAudioModule::Get(WorldContextObject ? WorldContextObject->GetWorld() : nullptr).SetBackgroundSoundVolume(InVolume, InFadeInTime);
}

float UAudioModuleStatics::GetBackgroundSoundPitch(const UObject* WorldContextObject)
{
	return UAudioModule::Get(WorldContextObject ? WorldContextObject->GetWorld() : nullptr).GetBackgroundSoundPitch();
}

void UAudioModuleStatics::SetBackgroundSoundPitch(const UObject* WorldContextObject, float InPitch, float InFadeInTime)
{
	UAudioModule::Get(WorldContextObject ? WorldContextObject->GetWorld() : nullptr).SetBackgroundSoundPitch(InPitch, InFadeInTime);
}

float UAudioModuleStatics::GetEnvironmentSoundVolume(const UObject* WorldContextObject)
{
	return UAudioModule::Get(WorldContextObject ? WorldContextObject->GetWorld() : nullptr).GetEnvironmentSoundVolume();
}

void UAudioModuleStatics::SetEnvironmentSoundVolume(const UObject* WorldContextObject, float InVolume, float InFadeInTime)
{
	UAudioModule::Get(WorldContextObject ? WorldContextObject->GetWorld() : nullptr).SetEnvironmentSoundVolume(InVolume, InFadeInTime);
}

float UAudioModuleStatics::GetEnvironmentSoundPitch(const UObject* WorldContextObject)
{
	return UAudioModule::Get(WorldContextObject ? WorldContextObject->GetWorld() : nullptr).GetEnvironmentSoundPitch();
}

void UAudioModuleStatics::SetEnvironmentSoundPitch(const UObject* WorldContextObject, float InPitch, float InFadeInTime)
{
	UAudioModule::Get(WorldContextObject ? WorldContextObject->GetWorld() : nullptr).SetEnvironmentSoundPitch(InPitch, InFadeInTime);
}

float UAudioModuleStatics::GetEffectSoundVolume(const UObject* WorldContextObject)
{
	return UAudioModule::Get(WorldContextObject ? WorldContextObject->GetWorld() : nullptr).GetEffectSoundVolume();
}

void UAudioModuleStatics::SetEffectSoundVolume(const UObject* WorldContextObject, float InVolume, float InFadeInTime)
{
	UAudioModule::Get(WorldContextObject ? WorldContextObject->GetWorld() : nullptr).SetEffectSoundVolume(InVolume, InFadeInTime);
}

float UAudioModuleStatics::GetEffectSoundPitch(const UObject* WorldContextObject)
{
	return UAudioModule::Get(WorldContextObject ? WorldContextObject->GetWorld() : nullptr).GetEffectSoundPitch();
}

void UAudioModuleStatics::SetEffectSoundPitch(const UObject* WorldContextObject, float InPitch, float InFadeInTime)
{
	UAudioModule::Get(WorldContextObject ? WorldContextObject->GetWorld() : nullptr).SetEffectSoundPitch(InPitch, InFadeInTime);
}
