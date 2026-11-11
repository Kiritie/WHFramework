// Fill out your copyright notice in the Description page of Project Settings.


#include "Audio/AudioModule.h"
#include "WHFrameworkCoreMacros.h"
#include "Common/LifecycleTypes.h"
#include "Common/LanguageTypes.h"

#include "Components/AudioComponent.h"
#include "Audio/AudioModuleStatics.h"
#include "Audio/AudioModuleNetworkComponent.h"
#include "Common/CommonModuleStatics.h"
#include "Event/EventModuleStatics.h"
#include "Event/Events/Audio/Event_SoundParamsChanged.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Main/MainModule.h"
#include "Main/WorldNetworkRecipients.h"
#include "Main/WorldNetworkRoute.h"
#include "Engine/NetConnection.h"
#include "Net/UnrealNetwork.h"
#include "SaveGame/SaveGameModuleStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

IMPLEMENTATION_MODULE(UAudioModule)

namespace
{
	FNetworkDispatchResult DispatchAudioToWorld(UAudioModule* Audio, TFunctionRef<void()> DispatchLocal, TFunctionRef<void(UAudioModuleNetworkComponent*, FGuid)> DispatchRemote)
    {
        if (!Audio || Audio->IsAudioClosing() || !Audio->GetModuleOwner() || !Audio->GetModuleOwner()->HasAuthority())
            return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::NotAuthorized);
        UWorld* World = Audio->GetWorld();
        if (!World) return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::InvalidContext);
        FNetworkDispatchResult Result;
        Result.WorldEpoch = WorldNetworkRoute::GetEpoch(World);
        const TArray<APlayerController*> Recipients = WorldNetworkRecipients::Gather(World);
        Result.bLocalDispatched = true;
        DispatchLocal();
        for (APlayerController* Controller : Recipients)
        {
            UAudioModuleNetworkComponent* Network = Controller ? Audio->GetModuleNetworkComponent<UAudioModuleNetworkComponent>(Controller) : nullptr;
            const FGuid Route = Network ? Network->GetRemoteWorldRoute() : FGuid();
            if (!Result.WorldEpoch.IsValid() || !Route.IsValid())
            {
                ++Result.RemoteUnavailable;
                continue;
            }
            DispatchRemote(Network, Route);
            ++Result.RemoteDispatched;
        }
        Result.Status = Result.RemoteUnavailable ? ENetworkDispatchStatus::PartiallyDispatched :
            Result.RemoteDispatched ? ENetworkDispatchStatus::RpcDispatched : ENetworkDispatchStatus::LocalDispatched;
        return Result;
    }

    FNetworkDispatchResult LocalAudioDispatched(UWorld* World)
    {
        FNetworkDispatchResult Result;
        Result.bLocalDispatched = true;
        Result.Status = ENetworkDispatchStatus::LocalDispatched;
        Result.WorldEpoch = WorldNetworkRoute::GetEpoch(World);
        return Result;
    }

}

// Sets default values
UAudioModule::UAudioModule()
{
	ModuleName = FName("AudioModule");
	ModuleDisplayName = FText::FromString(TEXT("Audio Module"));

	
	ModuleNetworkComponent = UAudioModuleNetworkComponent::StaticClass();

	LanguageTypes = TArray<FLanguageType>();
	LanguageType = 0;

	static ConstructorHelpers::FObjectFinder<USoundMix> GlobalSoundMixFinder(TEXT("/Script/Engine.SoundMix'/WHFramework/Sounds/Audio/Mix/SCM_Global.SCM_Global'"));
	if(GlobalSoundMixFinder.Succeeded())
	{
		GlobalSoundMix = GlobalSoundMixFinder.Object;
	}
	static ConstructorHelpers::FObjectFinder<USoundClass> GlobalSoundClassFinder(TEXT("/Script/Engine.SoundClass'/WHFramework/Sounds/Audio/Class/SC_Global.SC_Global'"));
	if(GlobalSoundClassFinder.Succeeded())
	{
		GlobalSoundClass = GlobalSoundClassFinder.Object;
	}
	GlobalSoundParams = FSoundParams(1.f, 1.f);

	static ConstructorHelpers::FObjectFinder<USoundMix> BackgroundSoundMixFinder(TEXT("/Script/Engine.SoundMix'/WHFramework/Sounds/Audio/Mix/SCM_Background.SCM_Background'"));
	if(BackgroundSoundMixFinder.Succeeded())
	{
		BackgroundSoundMix = BackgroundSoundMixFinder.Object;
	}
	static ConstructorHelpers::FObjectFinder<USoundClass> BackgroundSoundClassFinder(TEXT("/Script/Engine.SoundClass'/WHFramework/Sounds/Audio/Class/SC_Background.SC_Background'"));
	if(BackgroundSoundClassFinder.Succeeded())
	{
		BackgroundSoundClass = BackgroundSoundClassFinder.Object;
	}
	BackgroundSoundParams = FSoundParams(1.f, 1.f);

	static ConstructorHelpers::FObjectFinder<USoundMix> EnvironmentSoundMixFinder(TEXT("/Script/Engine.SoundMix'/WHFramework/Sounds/Audio/Mix/SCM_Environment.SCM_Environment'"));
	if(EnvironmentSoundMixFinder.Succeeded())
	{
		EnvironmentSoundMix = EnvironmentSoundMixFinder.Object;
	}
	static ConstructorHelpers::FObjectFinder<USoundClass> EnvironmentSoundClassFinder(TEXT("/Script/Engine.SoundClass'/WHFramework/Sounds/Audio/Class/SC_Environment.SC_Environment'"));
	if(EnvironmentSoundClassFinder.Succeeded())
	{
		EnvironmentSoundClass = EnvironmentSoundClassFinder.Object;
	}
	EnvironmentSoundParams = FSoundParams(1.f, 1.f);

	static ConstructorHelpers::FObjectFinder<USoundMix> EffectSoundMixFinder(TEXT("/Script/Engine.SoundMix'/WHFramework/Sounds/Audio/Mix/SCM_Effect.SCM_Effect'"));
	if(EffectSoundMixFinder.Succeeded())
	{
		EffectSoundMix = EffectSoundMixFinder.Object;
	}
	static ConstructorHelpers::FObjectFinder<USoundClass> EffectSoundClassFinder(TEXT("/Script/Engine.SoundClass'/WHFramework/Sounds/Audio/Class/SC_Effect.SC_Effect'"));
	if(EffectSoundClassFinder.Succeeded())
	{
		EffectSoundClass = EffectSoundClassFinder.Object;
	}
	EffectSoundParams = FSoundParams(1.f, 1.f);
}

UAudioModule::~UAudioModule()
{
}

#if WITH_EDITOR
void UAudioModule::OnGenerate()
{
	Super::OnGenerate();

}

void UAudioModule::OnDestroy()
{
	Super::OnDestroy();
}
#endif

void UAudioModule::OnInitialize()
{
	bAudioClosing = false;
	Super::OnInitialize();
}

void UAudioModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);

	if(PHASEC(InPhase, EPhase::Final))
	{
		SetSoundParams(GlobalSoundMix, GlobalSoundClass, GlobalSoundParams, 0.f);
		SetSoundParams(BackgroundSoundMix, BackgroundSoundClass, BackgroundSoundParams, 0.f);
		SetSoundParams(EnvironmentSoundMix, EnvironmentSoundClass, EnvironmentSoundParams, 0.f);
		SetSoundParams(EffectSoundMix, EffectSoundClass, EffectSoundParams, 0.f);
	}
}

void UAudioModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
	Super::OnRefresh(DeltaSeconds, bInEditor);
}

void UAudioModule::OnPause()
{
	Super::OnPause();
}

void UAudioModule::OnUnPause()
{
	Super::OnUnPause();
}

void UAudioModule::OnTermination(EPhase InPhase)
{
	bAudioClosing = true;
	Super::OnTermination(InPhase);
}

void UAudioModule::LoadData(const FParameter& InSaveData, EPhase InPhase)
{
	auto& SaveData = InSaveData.GetRef<FAudioModuleSaveData>();
	if(SaveData.IsSaved())
	{
		GlobalSoundParams = SaveData.GlobalSoundParams;
		BackgroundSoundParams = SaveData.BackgroundSoundParams;
		EnvironmentSoundParams = SaveData.EnvironmentSoundParams;
		EffectSoundParams = SaveData.EffectSoundParams;

		SetSoundParams(GlobalSoundMix, GlobalSoundClass, GlobalSoundParams, 0.f);
		SetSoundParams(BackgroundSoundMix, BackgroundSoundClass, BackgroundSoundParams, 0.f);
		SetSoundParams(EnvironmentSoundMix, EnvironmentSoundClass, EnvironmentSoundParams, 0.f);
		SetSoundParams(EffectSoundMix, EffectSoundClass, EffectSoundParams, 0.f);
	}
}

void UAudioModule::UnloadData(EPhase InPhase)
{
}

FParameter UAudioModule::ToData()
{
	FAudioModuleSaveData SaveData;

	SaveData.GlobalSoundParams = GlobalSoundParams;
	SaveData.BackgroundSoundParams = BackgroundSoundParams;
	SaveData.EnvironmentSoundParams = EnvironmentSoundParams;
	SaveData.EffectSoundParams = EffectSoundParams;
	
	return FParameter(MoveTemp(SaveData));
}

FString UAudioModule::GetModuleDebugMessage()
{
	FString DebugMessage;
	for(auto Iter : SingleSoundInfos)
	{
		DebugMessage.Appendf(TEXT("%s-%s(%s)\n"), *Iter.Key.ToString(), *Iter.Value.Sound->GetName(), *UCommonModuleStatics::GetEnumAuthoredNameByValue(this, TEXT("/Script/Engine.EAudioComponentPlayState"), (int32)Iter.Value.Audio->GetPlayState()));
	}
	DebugMessage.RemoveFromEnd(TEXT("\n"));
	if(!DebugMessage.IsEmpty())
	{
		return DebugMessage;
	}
	return Super::GetModuleDebugMessage();
}

FNetworkDispatchResult UAudioModule::PlaySound2D(USoundBase* InSound, float InVolume, bool bMulticast, APlayerController* InController)
{
	UWorld* World = GetWorld();
	if (!::IsValid(World) || World->bIsTearingDown || IsAudioClosing()) return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::InvalidContext);
	if (!::IsValid(InSound) || !FMath::IsFinite(InVolume) || InVolume < 0.f) return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::InvalidRequest);
	if (!bMulticast)
	{
		UGameplayStatics::PlaySound2D(this, InSound, InVolume);
		return LocalAudioDispatched(World);
	}
	if (!GetModuleOwner()) return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::ModuleUnavailable);
	if (GetModuleOwner()->HasAuthority()) return MultiPlaySound2D(InSound, InVolume);
	if (!::IsValid(InController) || !InController->IsLocalController() || InController->GetWorld() != World)
		return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::InvalidContext);
	UAudioModuleNetworkComponent* Network = GetModuleNetworkComponent<UAudioModuleNetworkComponent>(InController);
	if (!Network) return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::ModuleUnavailable);
	const FGuid Route = Network->MaintainLocalWorldRoute();
	if (!Route.IsValid()) return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::RouteUnavailable);
	Network->ServerPlaySound2DMulticast(Route, InSound, InVolume);
	FNetworkDispatchResult Result;
	Result.Status = ENetworkDispatchStatus::RpcDispatched;
	Result.WorldEpoch = WorldNetworkRoute::GetEpoch(World);
	Result.RemoteDispatched = 1;
	return Result;
}

FNetworkDispatchResult UAudioModule::MultiPlaySound2D(USoundBase* InSound, float InVolume)
{
	if (!::IsValid(InSound) || !FMath::IsFinite(InVolume) || InVolume < 0.f) return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::InvalidRequest);
	return DispatchAudioToWorld(this, [this, InSound, InVolume] { UGameplayStatics::PlaySound2D(this, InSound, InVolume); },
		[InSound, InVolume](UAudioModuleNetworkComponent* Network, FGuid Route) { Network->ClientPlaySound2D(Route, InSound, InVolume); });
}

FNetworkDispatchResult UAudioModule::PlaySoundAtLocation(USoundBase* InSound, FVector InLocation, float InVolume, bool bMulticast, APlayerController* InController)
{
	UWorld* World = GetWorld();
	if (!::IsValid(World) || World->bIsTearingDown || IsAudioClosing()) return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::InvalidContext);
	if (!::IsValid(InSound) || InLocation.ContainsNaN() || !FMath::IsFinite(InVolume) || InVolume < 0.f) return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::InvalidRequest);
	if (!bMulticast)
	{
		UGameplayStatics::PlaySoundAtLocation(this, InSound, InLocation, InVolume);
		return LocalAudioDispatched(World);
	}
	if (!GetModuleOwner()) return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::ModuleUnavailable);
	if (GetModuleOwner()->HasAuthority()) return MultiPlaySoundAtLocation(InSound, InLocation, InVolume);
	if (!::IsValid(InController) || !InController->IsLocalController() || InController->GetWorld() != World)
		return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::InvalidContext);
	UAudioModuleNetworkComponent* Network = GetModuleNetworkComponent<UAudioModuleNetworkComponent>(InController);
	if (!Network) return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::ModuleUnavailable);
	const FGuid Route = Network->MaintainLocalWorldRoute();
	if (!Route.IsValid()) return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::RouteUnavailable);
	Network->ServerPlaySoundAtLocationMulticast(Route, InSound, InLocation, InVolume);
	FNetworkDispatchResult Result;
	Result.Status = ENetworkDispatchStatus::RpcDispatched;
	Result.WorldEpoch = WorldNetworkRoute::GetEpoch(World);
	Result.RemoteDispatched = 1;
	return Result;
}

FNetworkDispatchResult UAudioModule::MultiPlaySoundAtLocation(USoundBase* InSound, FVector InLocation, float InVolume)
{
	if (!::IsValid(InSound) || InLocation.ContainsNaN() || !FMath::IsFinite(InVolume) || InVolume < 0.f)
		return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::InvalidRequest);
	return DispatchAudioToWorld(this, [this, InSound, InLocation, InVolume] { UGameplayStatics::PlaySoundAtLocation(this, InSound, InLocation, InVolume); },
		[InSound, InLocation, InVolume](UAudioModuleNetworkComponent* Network, FGuid Route) { Network->ClientPlaySoundAtLocation(Route, InSound, InLocation, InVolume); });
}

bool UAudioModule::CanRequestNetworkSound(const APlayerController* InController, const USoundBase* InSound) const
{
	if (IsAudioClosing()) return false;
	if (!::IsValid(InController) || InController->GetWorld() != GetWorld() || !::IsValid(InSound)) return false;
	const FSoftObjectPath SoundPath(InSound);
	return ClientMulticastAllowedSounds.ContainsByPredicate([&SoundPath](const TSoftObjectPtr<USoundBase>& Allowed)
	{
		return Allowed.ToSoftObjectPath() == SoundPath;
	});
}

FSingleSoundInfo UAudioModule::GetSingleSoundInfo(const FSingleSoundHandle& InHandle, APlayerController* InController) const
{
	FSingleSoundHandle Resolved = InHandle;
	if (InHandle.IsPending())
	{
		UAudioModuleNetworkComponent* Network = GetModuleNetworkComponent<UAudioModuleNetworkComponent>(InController);
		Resolved = Network ? Network->ResolveRequest(InHandle.GetRequestId()) : FSingleSoundHandle();
	}
	const FSingleSoundInfo* Info = SingleSoundInfos.Find(Resolved);
	return Info ? *Info : FSingleSoundInfo();
}

EAudioNetworkSoundState UAudioModule::GetSingleSoundRequestState(const FSingleSoundHandle& InHandle, APlayerController* InController) const
{
	UAudioModuleNetworkComponent* Network = InHandle.IsPending() ? GetModuleNetworkComponent<UAudioModuleNetworkComponent>(InController) : nullptr;
	return Network ? Network->GetRequestState(InHandle.GetRequestId()) : EAudioNetworkSoundState::Unknown;
}

FNetworkDispatchResult UAudioModule::GetSingleSoundDispatchResult(const FSingleSoundHandle& InHandle, APlayerController* InController) const
{
	UAudioModuleNetworkComponent* Network = InHandle.IsPending() ? GetModuleNetworkComponent<UAudioModuleNetworkComponent>(InController) : nullptr;
	return Network ? Network->GetRequestDispatchResult(InHandle.GetRequestId()) : FNetworkDispatchResult::Failure(ENetworkDispatchStatus::InvalidRequest);
}

void UAudioModule::BindSingleSoundCompletion(const FSingleSoundHandle& InHandle, const FOnSoundPlayFinishDelegate& InDelegate)
{
	if (IsAudioClosing()) return;
	if (!InHandle.IsValid() || !InDelegate.IsBound()) return;
	if (FSingleSoundInfo* Info = SingleSoundInfos.Find(InHandle); Info && Info->Audio)
		Info->Audio->OnAudioFinished.Add(InDelegate);
	else PendingSoundDelegates.Add(InHandle, InDelegate);
}

FSingleSoundHandle UAudioModule::AllocateNetworkSoundHandle()
{
	if (IsAudioClosing()) return FSingleSoundHandle();
	return GetModuleOwner() && GetModuleOwner()->HasAuthority() && NetworkSoundHandle < MAX_int32 ? FSingleSoundHandle(NetworkSoundHandle++, true) : FSingleSoundHandle();
}

FSoundPlaybackResult UAudioModule::PlaySingleSound2D(USoundBase* InSound, float InVolume, bool bMulticast, APlayerController* InController)
{
	FSoundPlaybackResult Result;
	UWorld* World = GetWorld();
	if (!::IsValid(World) || World->bIsTearingDown || IsAudioClosing()) { Result.Dispatch.Status = ENetworkDispatchStatus::InvalidContext; return Result; }
	if (!::IsValid(InSound) || !FMath::IsFinite(InVolume) || InVolume < 0.f) { Result.Dispatch.Status = ENetworkDispatchStatus::InvalidRequest; return Result; }
	if (bMulticast && !GetModuleOwner()) { Result.Dispatch.Status = ENetworkDispatchStatus::ModuleUnavailable; return Result; }
	if (bMulticast && !GetModuleOwner()->HasAuthority())
	{
		if (!::IsValid(InController) || !InController->IsLocalController() || InController->GetWorld() != World)
		{ Result.Dispatch.Status = ENetworkDispatchStatus::InvalidContext; return Result; }
		UAudioModuleNetworkComponent* Network = GetModuleNetworkComponent<UAudioModuleNetworkComponent>(InController);
		if (!Network) { Result.Dispatch.Status = ENetworkDispatchStatus::ModuleUnavailable; return Result; }
		const FGuid Route = Network->MaintainLocalWorldRoute();
		if (!Route.IsValid()) { Result.Dispatch.Status = ENetworkDispatchStatus::RouteUnavailable; return Result; }
		const FGuid RequestId = FGuid::NewGuid();
		if (!Network->RegisterRequest(RequestId)) { Result.Dispatch.Status = ENetworkDispatchStatus::CapacityExceeded; return Result; }
		Network->ServerPlaySingleSound2DMulticast(Route, RequestId, InSound, InVolume);
		Result.Handle = FSingleSoundHandle::Pending(RequestId);
		Result.Dispatch.Status = ENetworkDispatchStatus::RpcDispatched;
		Result.Dispatch.WorldEpoch = WorldNetworkRoute::GetEpoch(World);
		Result.Dispatch.RemoteDispatched = 1;
		return Result;
	}
	if (bMulticast) Result.Handle = AllocateNetworkSoundHandle();
	else if (LocalSoundHandle < MAX_int32) Result.Handle = FSingleSoundHandle(LocalSoundHandle++);
	if (!Result.Handle.IsValid()) { Result.Dispatch.Status = ENetworkDispatchStatus::CapacityExceeded; return Result; }
	if (bMulticast) Result.Dispatch = MultiPlaySingleSound2D(Result.Handle, InSound, InVolume);
	else { PlaySingleSound2DImpl(Result.Handle, InSound, InVolume); Result.Dispatch = LocalAudioDispatched(World); }
	return Result;
}

FSoundPlaybackResult UAudioModule::PlaySingleSound2DWithDelegate(USoundBase* InSound, const FOnSoundPlayFinishDelegate& InOnSoundPlayFinish, float InVolume, bool bMulticast, APlayerController* InController)
{

	FSoundPlaybackResult Result = PlaySingleSound2D(InSound, InVolume, bMulticast, InController);
	if (Result.Handle.IsPending())
	{
		if (UAudioModuleNetworkComponent* Network = GetModuleNetworkComponent<UAudioModuleNetworkComponent>(InController)) Network->TrackCompletion(Result.Handle.GetRequestId(), InOnSoundPlayFinish);
	}
	else BindSingleSoundCompletion(Result.Handle, InOnSoundPlayFinish);
	return Result;
}

FNetworkDispatchResult UAudioModule::MultiPlaySingleSound2D(const FSingleSoundHandle& InHandle, USoundBase* InSound, float InVolume)
{
	if (!InHandle.IsValid() || InHandle.IsPending() || !::IsValid(InSound) || !FMath::IsFinite(InVolume) || InVolume < 0.f)
		return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::InvalidRequest);
	return DispatchAudioToWorld(this, [this, InHandle, InSound, InVolume] { PlaySingleSound2DImpl(InHandle, InSound, InVolume); },
		[InHandle, InSound, InVolume](UAudioModuleNetworkComponent* Network, FGuid Route) { Network->ClientPlaySingleSound2D(Route, InHandle, InSound, InVolume); });
}

FSoundPlaybackResult UAudioModule::PlaySingleSoundAtLocation(USoundBase* InSound, FVector InLocation, float InVolume, bool bMulticast, APlayerController* InController)
{
	FSoundPlaybackResult Result;
	UWorld* World = GetWorld();
	if (!::IsValid(World) || World->bIsTearingDown || IsAudioClosing()) { Result.Dispatch.Status = ENetworkDispatchStatus::InvalidContext; return Result; }
	if (!::IsValid(InSound) || InLocation.ContainsNaN() || !FMath::IsFinite(InVolume) || InVolume < 0.f)
	{ Result.Dispatch.Status = ENetworkDispatchStatus::InvalidRequest; return Result; }
	if (bMulticast && !GetModuleOwner()) { Result.Dispatch.Status = ENetworkDispatchStatus::ModuleUnavailable; return Result; }
	if (bMulticast && !GetModuleOwner()->HasAuthority())
	{
		if (!::IsValid(InController) || !InController->IsLocalController() || InController->GetWorld() != World)
		{ Result.Dispatch.Status = ENetworkDispatchStatus::InvalidContext; return Result; }
		UAudioModuleNetworkComponent* Network = GetModuleNetworkComponent<UAudioModuleNetworkComponent>(InController);
		if (!Network) { Result.Dispatch.Status = ENetworkDispatchStatus::ModuleUnavailable; return Result; }
		const FGuid Route = Network->MaintainLocalWorldRoute();
		if (!Route.IsValid()) { Result.Dispatch.Status = ENetworkDispatchStatus::RouteUnavailable; return Result; }
		const FGuid RequestId = FGuid::NewGuid();
		if (!Network->RegisterRequest(RequestId)) { Result.Dispatch.Status = ENetworkDispatchStatus::CapacityExceeded; return Result; }
		Network->ServerPlaySingleSoundAtLocationMulticast(Route, RequestId, InSound, InLocation, InVolume);
		Result.Handle = FSingleSoundHandle::Pending(RequestId);
		Result.Dispatch.Status = ENetworkDispatchStatus::RpcDispatched;
		Result.Dispatch.WorldEpoch = WorldNetworkRoute::GetEpoch(World);
		Result.Dispatch.RemoteDispatched = 1;
		return Result;
	}
	if (bMulticast) Result.Handle = AllocateNetworkSoundHandle();
	else if (LocalSoundHandle < MAX_int32) Result.Handle = FSingleSoundHandle(LocalSoundHandle++);
	if (!Result.Handle.IsValid()) { Result.Dispatch.Status = ENetworkDispatchStatus::CapacityExceeded; return Result; }
	if (bMulticast) Result.Dispatch = MultiPlaySingleSoundAtLocation(Result.Handle, InSound, InLocation, InVolume);
	else { PlaySingleSoundAtLocationImpl(Result.Handle, InSound, InLocation, InVolume); Result.Dispatch = LocalAudioDispatched(World); }
	return Result;
}

FSoundPlaybackResult UAudioModule::PlaySingleSoundAtLocationWithDelegate(USoundBase* InSound, const FOnSoundPlayFinishDelegate& InOnSoundPlayFinish, FVector InLocation, float InVolume, bool bMulticast, APlayerController* InController)
{

	FSoundPlaybackResult Result = PlaySingleSoundAtLocation(InSound, InLocation, InVolume, bMulticast, InController);
	if (Result.Handle.IsPending())
	{
		if (UAudioModuleNetworkComponent* Network = GetModuleNetworkComponent<UAudioModuleNetworkComponent>(InController)) Network->TrackCompletion(Result.Handle.GetRequestId(), InOnSoundPlayFinish);
	}
	else BindSingleSoundCompletion(Result.Handle, InOnSoundPlayFinish);
	return Result;
}

FNetworkDispatchResult UAudioModule::MultiPlaySingleSoundAtLocation(const FSingleSoundHandle& InHandle, USoundBase* InSound, FVector InLocation, float InVolume)
{
	if (!InHandle.IsValid() || InHandle.IsPending() || !::IsValid(InSound) || InLocation.ContainsNaN() || !FMath::IsFinite(InVolume) || InVolume < 0.f)
		return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::InvalidRequest);
	return DispatchAudioToWorld(this, [this, InHandle, InSound, InLocation, InVolume] { PlaySingleSoundAtLocationImpl(InHandle, InSound, InLocation, InVolume); },
		[InHandle, InSound, InLocation, InVolume](UAudioModuleNetworkComponent* Network, FGuid Route) { Network->ClientPlaySingleSoundAtLocation(Route, InHandle, InSound, InLocation, InVolume); });
}

FNetworkDispatchResult UAudioModule::StopSingleSound(const FSingleSoundHandle& InHandle, bool bMulticast, APlayerController* InController)
{
	UWorld* World = GetWorld();
	if (!::IsValid(World) || World->bIsTearingDown || IsAudioClosing()) return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::InvalidContext);
	if (!InHandle.IsValid()) return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::InvalidRequest);
	if (!bMulticast)
	{
		if (InHandle.IsPending()) return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::InvalidRequest);
		if (!SingleSoundInfos.Contains(InHandle) && !PendingSoundDelegates.Contains(InHandle)) return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::NotFound);
		PendingSoundDelegates.Remove(InHandle);
		StopSingleSoundImpl(InHandle);
		return LocalAudioDispatched(World);
	}
	if (!GetModuleOwner()) return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::ModuleUnavailable);
	if (GetModuleOwner()->HasAuthority()) return MultiStopSingleSound(InHandle);
	if (!InHandle.IsPending() || !::IsValid(InController) || !InController->IsLocalController() || InController->GetWorld() != World)
		return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::NotAuthorized);
	UAudioModuleNetworkComponent* Network = GetModuleNetworkComponent<UAudioModuleNetworkComponent>(InController);
	if (!Network) return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::ModuleUnavailable);
	const FGuid Route = Network->MaintainLocalWorldRoute();
	if (!Route.IsValid()) return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::RouteUnavailable);
	Network->ServerStopSingleSoundMulticast(Route, InHandle.GetRequestId());
	Network->ForgetRequest(InHandle.GetRequestId());
	FNetworkDispatchResult Result;
	Result.Status = ENetworkDispatchStatus::RpcDispatched;
	Result.WorldEpoch = WorldNetworkRoute::GetEpoch(World);
	Result.RemoteDispatched = 1;
	return Result;
}

FNetworkDispatchResult UAudioModule::MultiStopSingleSound(const FSingleSoundHandle& InHandle)
{
	if (!InHandle.IsValid() || InHandle.IsPending()) return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::InvalidRequest);
	return DispatchAudioToWorld(this, [this, InHandle] { PendingSoundDelegates.Remove(InHandle); StopSingleSoundImpl(InHandle); },
		[InHandle](UAudioModuleNetworkComponent* Network, FGuid Route) { Network->ClientStopSingleSound(Route, InHandle); });
}

FNetworkDispatchResult UAudioModule::SetSingleSoundPaused(const FSingleSoundHandle& InHandle, bool bPaused, bool bMulticast, APlayerController* InController)
{
	UWorld* World = GetWorld();
	if (!::IsValid(World) || World->bIsTearingDown || IsAudioClosing()) return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::InvalidContext);
	if (!InHandle.IsValid()) return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::InvalidRequest);
	if (!bMulticast)
	{
		if (InHandle.IsPending()) return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::InvalidRequest);
		if (!SingleSoundInfos.Contains(InHandle)) return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::NotFound);
		SetSingleSoundPausedImpl(InHandle, bPaused);
		return LocalAudioDispatched(World);
	}
	if (!GetModuleOwner()) return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::ModuleUnavailable);
	if (GetModuleOwner()->HasAuthority()) return MultiSetSingleSoundPaused(InHandle, bPaused);
	if (!InHandle.IsPending() || !::IsValid(InController) || !InController->IsLocalController() || InController->GetWorld() != World)
		return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::NotAuthorized);
	UAudioModuleNetworkComponent* Network = GetModuleNetworkComponent<UAudioModuleNetworkComponent>(InController);
	if (!Network) return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::ModuleUnavailable);
	const FGuid Route = Network->MaintainLocalWorldRoute();
	if (!Route.IsValid()) return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::RouteUnavailable);
	Network->ServerSetSingleSoundPausedMulticast(Route, InHandle.GetRequestId(), bPaused);
	FNetworkDispatchResult Result;
	Result.Status = ENetworkDispatchStatus::RpcDispatched;
	Result.WorldEpoch = WorldNetworkRoute::GetEpoch(World);
	Result.RemoteDispatched = 1;
	return Result;
}

FNetworkDispatchResult UAudioModule::MultiSetSingleSoundPaused(const FSingleSoundHandle& InHandle, bool bPaused)
{
	if (!InHandle.IsValid() || InHandle.IsPending()) return FNetworkDispatchResult::Failure(ENetworkDispatchStatus::InvalidRequest);
	return DispatchAudioToWorld(this, [this, InHandle, bPaused] { SetSingleSoundPausedImpl(InHandle, bPaused); },
		[InHandle, bPaused](UAudioModuleNetworkComponent* Network, FGuid Route) { Network->ClientSetSingleSoundPaused(Route, InHandle, bPaused); });
}

UAudioComponent* UAudioModule::PlaySingleSound2DImpl(const FSingleSoundHandle& InHandle, USoundBase* InSound, float InVolume)
{
	StopSingleSoundImpl(InHandle);
	if(!SingleSoundInfos.Contains(InHandle))
	{
		UAudioComponent* AudioComponent = UGameplayStatics::SpawnSound2D(this, InSound, InVolume);
		if(AudioComponent)
		{
			AudioComponent->Play();
		}
		SingleSoundInfos.Add(InHandle, FSingleSoundInfo(AudioComponent, InSound));
		if (FOnSoundPlayFinishDelegate* Delegate = PendingSoundDelegates.Find(InHandle); Delegate && AudioComponent)
		{
			AudioComponent->OnAudioFinished.Add(*Delegate);
			PendingSoundDelegates.Remove(InHandle);
		}
	}
	return SingleSoundInfos[InHandle].Audio;
}

UAudioComponent* UAudioModule::PlaySingleSoundAtLocationImpl(const FSingleSoundHandle& InHandle, USoundBase* InSound, FVector InLocation, float InVolume)
{
	StopSingleSoundImpl(InHandle);
	if(!SingleSoundInfos.Contains(InHandle))
	{
		UAudioComponent* AudioComponent = UGameplayStatics::SpawnSoundAtLocation(this, InSound, InLocation, FRotator::ZeroRotator, InVolume);
		if(AudioComponent)
		{
			AudioComponent->Play();
		}
		SingleSoundInfos.Add(InHandle, FSingleSoundInfo(AudioComponent, InSound));
		if (FOnSoundPlayFinishDelegate* Delegate = PendingSoundDelegates.Find(InHandle); Delegate && AudioComponent)
		{
			AudioComponent->OnAudioFinished.Add(*Delegate);
			PendingSoundDelegates.Remove(InHandle);
		}
	}
	return SingleSoundInfos[InHandle].Audio;
}

void UAudioModule::StopSingleSoundImpl(const FSingleSoundHandle& InHandle)
{
	FSingleSoundInfo Info;
	if (SingleSoundInfos.RemoveAndCopyValue(InHandle, Info) && ::IsValid(Info.Audio))
	{
		Info.Audio->Stop();
	}
}

void UAudioModule::SetSingleSoundPausedImpl(const FSingleSoundHandle& InHandle, bool bPaused)
{
	if(SingleSoundInfos.Contains(InHandle))
	{
		if(SingleSoundInfos[InHandle].IsValid())
		{
			SingleSoundInfos[InHandle].Audio->SetPaused(bPaused);
		}
	}
}

void UAudioModule::SetSoundParams(USoundMix* InSoundMix, USoundClass* InSoundClass, const FSoundParams& InParams, float InFadeInTime)
{
	if (IsAudioClosing()) return;
	if(InSoundMix && InSoundClass)
	{
		UGameplayStatics::SetSoundMixClassOverride(this, InSoundMix, InSoundClass, InParams.Volume, InParams.Pitch, InFadeInTime);
		UGameplayStatics::PushSoundMixModifier(this, InSoundMix);
		UEventModuleStatics::BroadcastEvent<FEventSoundParamsChanged>(this, { InSoundClass, InParams });
	}
}

void UAudioModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

}
