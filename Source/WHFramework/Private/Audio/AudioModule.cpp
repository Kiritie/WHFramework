// Fill out your copyright notice in the Description page of Project Settings.


#include "Audio/AudioModule.h"

#include "Components/AudioComponent.h"
#include "Audio/AudioModuleStatics.h"
#include "Audio/AudioModuleNetworkComponent.h"
#include "Common/CommonStatics.h"
#include "Event/EventModuleStatics.h"
#include "Event/Handle/Audio/EventHandle_SoundParamsChanged.h"
#include "Kismet/GameplayStatics.h"
#include "Main/MainModule.h"
#include "Net/UnrealNetwork.h"
#include "SaveGame/SaveGameModuleStatics.h"
#include "SaveGame/Module/AudioSaveGame.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

IMPLEMENTATION_MODULE(UAudioModule)

// Sets default values
UAudioModule::UAudioModule()
{
	ModuleName = FName("AudioModule");
	ModuleDisplayName = FText::FromString(TEXT("Audio Module"));

	ModuleSaveGame = UAudioSaveGame::StaticClass();
	
	ModuleNetworkComponent = UAudioModuleNetworkComponent::StaticClass();

	static ConstructorHelpers::FObjectFinder<USoundMix> GlobalSoundMixFinder(TEXT("/Script/Engine.SoundMix'/WHFramework/Audio/Sounds/Mix/SCM_Global.SCM_Global'"));
	if(GlobalSoundMixFinder.Succeeded())
	{
		GlobalSoundMix = GlobalSoundMixFinder.Object;
	}
	static ConstructorHelpers::FObjectFinder<USoundClass> GlobalSoundClassFinder(TEXT("/Script/Engine.SoundClass'/WHFramework/Audio/Sounds/Class/SC_Global.SC_Global'"));
	if(GlobalSoundClassFinder.Succeeded())
	{
		GlobalSoundClass = GlobalSoundClassFinder.Object;
	}
	GlobalSoundParams = FSoundParams(1.f, 1.f);

	static ConstructorHelpers::FObjectFinder<USoundMix> BackgroundSoundMixFinder(TEXT("/Script/Engine.SoundMix'/WHFramework/Audio/Sounds/Mix/SCM_Background.SCM_Background'"));
	if(BackgroundSoundMixFinder.Succeeded())
	{
		BackgroundSoundMix = BackgroundSoundMixFinder.Object;
	}
	static ConstructorHelpers::FObjectFinder<USoundClass> BackgroundSoundClassFinder(TEXT("/Script/Engine.SoundClass'/WHFramework/Audio/Sounds/Class/SC_Background.SC_Background'"));
	if(BackgroundSoundClassFinder.Succeeded())
	{
		BackgroundSoundClass = BackgroundSoundClassFinder.Object;
	}
	BackgroundSoundParams = FSoundParams(1.f, 1.f);

	static ConstructorHelpers::FObjectFinder<USoundMix> EnvironmentSoundMixFinder(TEXT("/Script/Engine.SoundMix'/WHFramework/Audio/Sounds/Mix/SCM_Environment.SCM_Environment'"));
	if(EnvironmentSoundMixFinder.Succeeded())
	{
		EnvironmentSoundMix = EnvironmentSoundMixFinder.Object;
	}
	static ConstructorHelpers::FObjectFinder<USoundClass> EnvironmentSoundClassFinder(TEXT("/Script/Engine.SoundClass'/WHFramework/Audio/Sounds/Class/SC_Environment.SC_Environment'"));
	if(EnvironmentSoundClassFinder.Succeeded())
	{
		EnvironmentSoundClass = EnvironmentSoundClassFinder.Object;
	}
	EnvironmentSoundParams = FSoundParams(1.f, 1.f);

	static ConstructorHelpers::FObjectFinder<USoundMix> EffectSoundMixFinder(TEXT("/Script/Engine.SoundMix'/WHFramework/Audio/Sounds/Mix/SCM_Effect.SCM_Effect'"));
	if(EffectSoundMixFinder.Succeeded())
	{
		EffectSoundMix = EffectSoundMixFinder.Object;
	}
	static ConstructorHelpers::FObjectFinder<USoundClass> EffectSoundClassFinder(TEXT("/Script/Engine.SoundClass'/WHFramework/Audio/Sounds/Class/SC_Effect.SC_Effect'"));
	if(EffectSoundClassFinder.Succeeded())
	{
		EffectSoundClass = EffectSoundClassFinder.Object;
	}
	EffectSoundParams = FSoundParams(1.f, 1.f);
}

UAudioModule::~UAudioModule()
{
	TERMINATION_MODULE(UAudioModule)
}

#if WITH_EDITOR
void UAudioModule::OnGenerate()
{
	Super::OnGenerate();

	TERMINATION_MODULE(UAudioModule)
}

void UAudioModule::OnDestroy()
{
	Super::OnDestroy();
}
#endif

void UAudioModule::OnInitialize()
{
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
	Super::OnTermination(InPhase);
}

void UAudioModule::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	auto& SaveData = InSaveData->CastRef<FAudioModuleSaveData>();

	GlobalSoundParams = SaveData.GlobalSoundParams;
	BackgroundSoundParams = SaveData.BackgroundSoundParams;
	EnvironmentSoundParams = SaveData.EnvironmentSoundParams;
	EffectSoundParams = SaveData.EffectSoundParams;

	SetSoundParams(GlobalSoundMix, GlobalSoundClass, GlobalSoundParams, 0.f);
	SetSoundParams(BackgroundSoundMix, BackgroundSoundClass, BackgroundSoundParams, 0.f);
	SetSoundParams(EnvironmentSoundMix, EnvironmentSoundClass, EnvironmentSoundParams, 0.f);
	SetSoundParams(EffectSoundMix, EffectSoundClass, EffectSoundParams, 0.f);
}

void UAudioModule::UnloadData(EPhase InPhase)
{
}

FSaveData* UAudioModule::ToData()
{
	static FAudioModuleSaveData SaveData;
	SaveData = FAudioModuleSaveData();

	SaveData.GlobalSoundParams = GlobalSoundParams;
	SaveData.BackgroundSoundParams = BackgroundSoundParams;
	SaveData.EnvironmentSoundParams = EnvironmentSoundParams;
	SaveData.EffectSoundParams = EffectSoundParams;
	
	return &SaveData;
}

FString UAudioModule::GetModuleDebugMessage()
{
	FString DebugMessage;
	for(auto Iter : SingleSoundInfos)
	{
		DebugMessage.Appendf(TEXT("%s-%s(%s)\n"), *Iter.Key.ToString(), *Iter.Value.Sound->GetName(), *UCommonStatics::GetEnumValueAuthoredName(TEXT("/Script/Engine.EAudioComponentPlayState"), (int32)Iter.Value.Audio->GetPlayState()));
	}
	DebugMessage.RemoveFromEnd(TEXT("\n"));
	return DebugMessage;
}

void UAudioModule::PlaySound2D(USoundBase* InSound, float InVolume, bool bMulticast)
{
	if(bMulticast)
	{
		if(GetModuleOwner()->HasAuthority())
		{
			MultiPlaySound2D(InSound, InVolume);
		}
		else if(UAudioModuleNetworkComponent* AudioModuleNetworkComponent = GetModuleNetworkComponent<UAudioModuleNetworkComponent>())
		{
			AudioModuleNetworkComponent->ServerPlaySound2DMulticast(InSound, InVolume);
		}
		return;
	}
	UGameplayStatics::PlaySound2D(this, InSound, InVolume);
}

void UAudioModule::MultiPlaySound2D_Implementation(USoundBase* InSound, float InVolume)
{
	PlaySound2D(InSound, InVolume, false);
}

void UAudioModule::PlaySoundAtLocation(USoundBase* InSound, FVector InLocation, float InVolume, bool bMulticast)
{
	if(bMulticast)
	{
		if(GetModuleOwner()->HasAuthority())
		{
			MultiPlaySoundAtLocation(InSound, InLocation, InVolume);
		}
		else if(UAudioModuleNetworkComponent* AudioModuleNetworkComponent = GetModuleNetworkComponent<UAudioModuleNetworkComponent>())
		{
			AudioModuleNetworkComponent->ServerPlaySoundAtLocationMulticast(InSound, InLocation, InVolume);
		}
		return;
	}
	UGameplayStatics::PlaySoundAtLocation(this, InSound, InLocation, InVolume);
}

void UAudioModule::MultiPlaySoundAtLocation_Implementation(USoundBase* InSound, FVector InLocation, float InVolume)
{
	PlaySoundAtLocation(InSound, InLocation, InVolume, false);
}

FSingleSoundInfo UAudioModule::GetSingleSoundInfo(const FSingleSoundHandle& InHandle) const
{
	if(SingleSoundInfos.Contains(InHandle))
	{
		return SingleSoundInfos[InHandle];
	}
	return FSingleSoundInfo();
}

FSingleSoundHandle UAudioModule::PlaySingleSound2D(USoundBase* InSound, float InVolume, bool bMulticast)
{
	const FSingleSoundHandle Handle(SingleSoundHandle++);
	if(bMulticast)
	{
		if(GetModuleOwner()->HasAuthority())
		{
			MultiPlaySingleSound2D(Handle, InSound, InVolume);
		}
		else if(UAudioModuleNetworkComponent* AudioModuleNetworkComponent = GetModuleNetworkComponent<UAudioModuleNetworkComponent>())
		{
			AudioModuleNetworkComponent->ServerPlaySingleSound2DMulticast(Handle, InSound, InVolume);
		}
		return Handle;
	}
	PlaySingleSound2DImpl(Handle, InSound, InVolume);
	return Handle;
}

FSingleSoundHandle UAudioModule::PlaySingleSound2DWithDelegate(USoundBase* InSound, const FOnSoundPlayFinishDelegate& InOnSoundPlayFinish, float InVolume, bool bMulticast)
{
	const FSingleSoundHandle Handle(SingleSoundHandle++);
	if(bMulticast)
	{
		if(GetModuleOwner()->HasAuthority())
		{
			MultiPlaySingleSound2D(Handle, InSound, InVolume);
		}
		else if(UAudioModuleNetworkComponent* AudioModuleNetworkComponent = GetModuleNetworkComponent<UAudioModuleNetworkComponent>())
		{
			AudioModuleNetworkComponent->ServerPlaySingleSound2DMulticast(Handle, InSound, InVolume);
		}
		return Handle;
	}
	if(UAudioComponent* AudioComponent = PlaySingleSound2DImpl(Handle, InSound, InVolume))
	{
		AudioComponent->OnAudioFinished.Add(InOnSoundPlayFinish);
	}
	return Handle;
}

void UAudioModule::MultiPlaySingleSound2D_Implementation(const FSingleSoundHandle& InHandle, USoundBase* InSound, float InVolume)
{
	PlaySingleSound2DImpl(InHandle, InSound, InVolume);
}

FSingleSoundHandle UAudioModule::PlaySingleSoundAtLocation(USoundBase* InSound, FVector InLocation, float InVolume, bool bMulticast)
{
	const FSingleSoundHandle Handle(SingleSoundHandle++);
	if(bMulticast)
	{
		if(GetModuleOwner()->HasAuthority())
		{
			MultiPlaySingleSoundAtLocation(Handle, InSound, InLocation, InVolume);
		}
		else if(UAudioModuleNetworkComponent* AudioModuleNetworkComponent = GetModuleNetworkComponent<UAudioModuleNetworkComponent>())
		{
			AudioModuleNetworkComponent->ServerPlaySingleSoundAtLocationMulticast(Handle, InSound, InLocation, InVolume);
		}
		return Handle;
	}
	PlaySingleSoundAtLocationImpl(Handle, InSound, InLocation, InVolume);
	return Handle;
}

FSingleSoundHandle UAudioModule::PlaySingleSoundAtLocationWithDelegate(USoundBase* InSound, const FOnSoundPlayFinishDelegate& InOnSoundPlayFinish, FVector InLocation, float InVolume, bool bMulticast)
{
	const FSingleSoundHandle Handle(SingleSoundHandle++);
	if(bMulticast)
	{
		if(GetModuleOwner()->HasAuthority())
		{
			MultiPlaySingleSoundAtLocation(Handle, InSound, InLocation, InVolume);
		}
		else if(UAudioModuleNetworkComponent* AudioModuleNetworkComponent = GetModuleNetworkComponent<UAudioModuleNetworkComponent>())
		{
			AudioModuleNetworkComponent->ServerPlaySingleSoundAtLocationMulticast(Handle, InSound, InLocation, InVolume);
		}
		return Handle;
	}
	if(UAudioComponent* AudioComponent = PlaySingleSoundAtLocationImpl(Handle, InSound, InLocation, InVolume))
	{
		AudioComponent->OnAudioFinished.Add(InOnSoundPlayFinish);
	}
	return Handle;
}

void UAudioModule::MultiPlaySingleSoundAtLocation_Implementation(const FSingleSoundHandle& InHandle, USoundBase* InSound, FVector InLocation, float InVolume)
{
	PlaySingleSoundAtLocationImpl(InHandle, InSound, InLocation, InVolume);
}

void UAudioModule::StopSingleSound(const FSingleSoundHandle& InHandle, bool bMulticast)
{
	if(bMulticast)
	{
		if(GetModuleOwner()->HasAuthority())
		{
			MultiStopSingleSound(InHandle);
		}
		else if(UAudioModuleNetworkComponent* AudioModuleNetworkComponent = GetModuleNetworkComponent<UAudioModuleNetworkComponent>())
		{
			AudioModuleNetworkComponent->ServerStopSingleSoundMulticast(InHandle);
		}
		return;
	}
	StopSingleSoundImpl(InHandle);
}

void UAudioModule::MultiStopSingleSound_Implementation(const FSingleSoundHandle& InHandle)
{
	StopSingleSoundImpl(InHandle);
}

void UAudioModule::SetSingleSoundPaused(const FSingleSoundHandle& InHandle, bool bPaused, bool bMulticast)
{
	if(bMulticast)
	{
		if(GetModuleOwner()->HasAuthority())
		{
			MultiSetSingleSoundPaused(InHandle, bPaused);
		}
		else if(UAudioModuleNetworkComponent* AudioModuleNetworkComponent = GetModuleNetworkComponent<UAudioModuleNetworkComponent>())
		{
			AudioModuleNetworkComponent->ServerSetSingleSoundPausedMulticast(InHandle, bPaused);
		}
		return;
	}
	SetSingleSoundPausedImpl(InHandle, bPaused);
}

void UAudioModule::MultiSetSingleSoundPaused_Implementation(const FSingleSoundHandle& InHandle, bool bPaused)
{
	SetSingleSoundPausedImpl(InHandle, bPaused);
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
	}
	return SingleSoundInfos[InHandle].Audio;
}

void UAudioModule::StopSingleSoundImpl(const FSingleSoundHandle& InHandle)
{
	if(SingleSoundInfos.Contains(InHandle))
	{
		if(SingleSoundInfos[InHandle].IsValid())
		{
			SingleSoundInfos[InHandle].Audio->Stop();
		}
		SingleSoundInfos.Remove(InHandle);
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
	if(InSoundMix && InSoundClass)
	{
		UGameplayStatics::SetSoundMixClassOverride(this, InSoundMix, InSoundClass, InParams.Volume, InParams.Pitch, InFadeInTime);
		UGameplayStatics::PushSoundMixModifier(this, InSoundMix);
		UEventModuleStatics::BroadcastEvent<UEventHandle_SoundParamsChanged>(this, { InSoundClass, &const_cast<FSoundParams&>(InParams) });
	}
}

void UAudioModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UAudioModule, SingleSoundHandle);
}
