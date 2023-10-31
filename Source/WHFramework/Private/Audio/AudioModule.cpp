// Fill out your copyright notice in the Description page of Project Settings.


#include "Audio/AudioModule.h"

#include "Components/AudioComponent.h"
#include "Audio/AudioModuleBPLibrary.h"
#include "Audio/AudioModuleNetworkComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Main/MainModule.h"
#include "Net/UnrealNetwork.h"
#include "SaveGame/SaveGameModuleBPLibrary.h"
#include "SaveGame/Base/SaveGameBase.h"
#include "SaveGame/Module/AudioSaveGame.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

IMPLEMENTATION_MODULE(AAudioModule)

// Sets default values
AAudioModule::AAudioModule()
{
	ModuleName = FName("AudioModule");

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

AAudioModule::~AAudioModule()
{
	TERMINATION_MODULE(AAudioModule)
}

#if WITH_EDITOR
void AAudioModule::OnGenerate()
{
	Super::OnGenerate();
}

void AAudioModule::OnDestroy()
{
	Super::OnDestroy();
}
#endif

void AAudioModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void AAudioModule::OnPreparatory_Implementation(EPhase InPhase)
{
	Super::OnPreparatory_Implementation(InPhase);

	if(PHASEC(InPhase, EPhase::Lesser))
	{
		if(bModuleAutoSave)
		{
			Load();
		}
	}

	if(PHASEC(InPhase, EPhase::Final))
	{
		SetSoundParams(GlobalSoundMix, GlobalSoundClass, GlobalSoundParams, 0.f);
		SetSoundParams(BackgroundSoundMix, BackgroundSoundClass, BackgroundSoundParams, 0.f);
		SetSoundParams(EnvironmentSoundMix, EnvironmentSoundClass, EnvironmentSoundParams, 0.f);
		SetSoundParams(EffectSoundMix, EffectSoundClass, EffectSoundParams, 0.f);
	}
}

void AAudioModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void AAudioModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void AAudioModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}

void AAudioModule::OnTermination_Implementation(EPhase InPhase)
{
	Super::OnTermination_Implementation(InPhase);

	if(PHASEC(InPhase, EPhase::Lesser))
	{
		if(bModuleAutoSave)
		{
			Save();
		}
	}
}

void AAudioModule::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	auto& SaveData = InSaveData->CastRef<FAudioModuleSaveData>();

	GlobalSoundParams = SaveData.GlobalSoundParams;
	BackgroundSoundParams = SaveData.BackgroundSoundParams;
	EnvironmentSoundParams = SaveData.EnvironmentSoundParams;
	EffectSoundParams = SaveData.EffectSoundParams;
}

void AAudioModule::UnloadData(EPhase InPhase)
{
}

FSaveData* AAudioModule::ToData()
{
	static FAudioModuleSaveData SaveData;
	SaveData = FAudioModuleSaveData();

	SaveData.GlobalSoundParams = GlobalSoundParams;
	SaveData.BackgroundSoundParams = BackgroundSoundParams;
	SaveData.EnvironmentSoundParams = EnvironmentSoundParams;
	SaveData.EffectSoundParams = EffectSoundParams;
	
	return &SaveData;
}

void AAudioModule::PlaySound2D(USoundBase* InSound, float InVolume, bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
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

void AAudioModule::MultiPlaySound2D_Implementation(USoundBase* InSound, float InVolume)
{
	PlaySound2D(InSound, InVolume, false);
}

void AAudioModule::PlaySoundAtLocation(USoundBase* InSound, FVector InLocation, float InVolume, bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
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

void AAudioModule::MultiPlaySoundAtLocation_Implementation(USoundBase* InSound, FVector InLocation, float InVolume)
{
	PlaySoundAtLocation(InSound, InLocation, InVolume, false);
}

FSingleSoundInfo AAudioModule::GetSingleSoundInfo(const FSingleSoundHandle& InHandle) const
{
	if(SingleSoundInfos.Contains(InHandle))
	{
		SingleSoundInfos[InHandle];
	}
	return FSingleSoundInfo();
}

FSingleSoundHandle AAudioModule::PlaySingleSound2D(USoundBase* InSound, float InVolume, bool bMulticast)
{
	const FSingleSoundHandle Handle(SingleSoundHandle++);
	if(bMulticast)
	{
		if(HasAuthority())
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

FSingleSoundHandle AAudioModule::PlaySingleSound2DWithDelegate(USoundBase* InSound, const FOnSoundPlayFinishDelegate& InOnSoundPlayFinish, float InVolume, bool bMulticast)
{
	const FSingleSoundHandle Handle(SingleSoundHandle++);
	if(bMulticast)
	{
		if(HasAuthority())
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

void AAudioModule::MultiPlaySingleSound2D_Implementation(const FSingleSoundHandle& InHandle, USoundBase* InSound, float InVolume)
{
	PlaySingleSound2DImpl(InHandle, InSound, InVolume);
}

FSingleSoundHandle AAudioModule::PlaySingleSoundAtLocation(USoundBase* InSound, FVector InLocation, float InVolume, bool bMulticast)
{
	const FSingleSoundHandle Handle(SingleSoundHandle++);
	if(bMulticast)
	{
		if(HasAuthority())
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

FSingleSoundHandle AAudioModule::PlaySingleSoundAtLocationWithDelegate(USoundBase* InSound, const FOnSoundPlayFinishDelegate& InOnSoundPlayFinish, FVector InLocation, float InVolume, bool bMulticast)
{
	const FSingleSoundHandle Handle(SingleSoundHandle++);
	if(bMulticast)
	{
		if(HasAuthority())
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

void AAudioModule::MultiPlaySingleSoundAtLocation_Implementation(const FSingleSoundHandle& InHandle, USoundBase* InSound, FVector InLocation, float InVolume)
{
	PlaySingleSoundAtLocationImpl(InHandle, InSound, InLocation, InVolume);
}

void AAudioModule::StopSingleSound(const FSingleSoundHandle& InHandle, bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
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

void AAudioModule::MultiStopSingleSound_Implementation(const FSingleSoundHandle& InHandle)
{
	StopSingleSoundImpl(InHandle);
}

void AAudioModule::SetSingleSoundPaused(const FSingleSoundHandle& InHandle, bool bPaused, bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
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

void AAudioModule::MultiSetSingleSoundPaused_Implementation(const FSingleSoundHandle& InHandle, bool bPaused)
{
	SetSingleSoundPausedImpl(InHandle, bPaused);
}

UAudioComponent* AAudioModule::PlaySingleSound2DImpl(const FSingleSoundHandle& InHandle, USoundBase* InSound, float InVolume)
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

UAudioComponent* AAudioModule::PlaySingleSoundAtLocationImpl(const FSingleSoundHandle& InHandle, USoundBase* InSound, FVector InLocation, float InVolume)
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

void AAudioModule::StopSingleSoundImpl(const FSingleSoundHandle& InHandle)
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

void AAudioModule::SetSingleSoundPausedImpl(const FSingleSoundHandle& InHandle, bool bPaused)
{
	if(SingleSoundInfos.Contains(InHandle))
	{
		if(SingleSoundInfos[InHandle].IsValid())
		{
			SingleSoundInfos[InHandle].Audio->SetPaused(bPaused);
		}
	}
}

void AAudioModule::SetSoundParams(USoundMix* InSoundMix, USoundClass* InSoundClass, const FSoundParams& InParams, float InFadeInTime)
{
	if(InSoundMix && InSoundClass)
	{
		UGameplayStatics::SetSoundMixClassOverride(this, InSoundMix, InSoundClass, InParams.Volume, InParams.Pitch, InFadeInTime);
		UGameplayStatics::PushSoundMixModifier(this, InSoundMix);
	}
}

void AAudioModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AAudioModule, SingleSoundHandle);
}
