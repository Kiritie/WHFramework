// Fill out your copyright notice in the Description page of Project Settings.


#include "Audio/AudioModule.h"

#include "Debug/DebugModuleTypes.h"
#include "Components/AudioComponent.h"
#include "Audio/AudioModuleBPLibrary.h"
#include "Audio/AudioModuleNetworkComponent.h"
#include "Main/MainModule.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
		
IMPLEMENTATION_MODULE(AAudioModule)

// Sets default values
AAudioModule::AAudioModule()
{
	ModuleName = FName("AudioModule");
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
}

void AAudioModule::PlaySound2D(USoundBase* InSound, float InVolume, bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
		{
			MultiPlaySound2D(InSound, InVolume);
		}
		else if(UAudioModuleNetworkComponent* AudioModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UAudioModuleNetworkComponent>())
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
		else if(UAudioModuleNetworkComponent* AudioModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UAudioModuleNetworkComponent>())
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
		else if(UAudioModuleNetworkComponent* AudioModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UAudioModuleNetworkComponent>())
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
		else if(UAudioModuleNetworkComponent* AudioModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UAudioModuleNetworkComponent>())
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
		else if(UAudioModuleNetworkComponent* AudioModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UAudioModuleNetworkComponent>())
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
		else if(UAudioModuleNetworkComponent* AudioModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UAudioModuleNetworkComponent>())
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
		else if(UAudioModuleNetworkComponent* AudioModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UAudioModuleNetworkComponent>())
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
		else if(UAudioModuleNetworkComponent* AudioModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UAudioModuleNetworkComponent>())
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

void AAudioModule::SetGlobalSoundVolume(float InVolume, bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
		{
			MultiSetGlobalSoundVolume(InVolume);
		}
		else if(UAudioModuleNetworkComponent* AudioModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UAudioModuleNetworkComponent>())
		{
			AudioModuleNetworkComponent->ServerSetGlobalSoundVolumeMulticast(InVolume);
		}
		return;
	}
	SetSoundVolumeImpl(GlobalSoundMix, GlobalSoundClass, InVolume);
}

void AAudioModule::MultiSetGlobalSoundVolume_Implementation(float InVolume)
{
	SetGlobalSoundVolume(InVolume, false);
}

void AAudioModule::SetBackgroundSoundVolume(float InVolume, bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
		{
			MultiSetBackgroundSoundVolume(InVolume);
		}
		else if(UAudioModuleNetworkComponent* AudioModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UAudioModuleNetworkComponent>())
		{
			AudioModuleNetworkComponent->ServerSetBackgroundSoundVolumeMulticast(InVolume);
		}
		return;
	}
	SetSoundVolumeImpl(BackgroundSoundMix, BackgroundSoundClass, InVolume);
}

void AAudioModule::MultiSetBackgroundSoundVolume_Implementation(float InVolume)
{
	SetBackgroundSoundVolume(InVolume, false);
}

void AAudioModule::SetEnvironmentSoundVolume(float InVolume, bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
		{
			MultiSetEnvironmentSoundVolume(InVolume);
		}
		else if(UAudioModuleNetworkComponent* AudioModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UAudioModuleNetworkComponent>())
		{
			AudioModuleNetworkComponent->ServerSetEnvironmentSoundVolumeMulticast(InVolume);
		}
		return;
	}
	SetSoundVolumeImpl(EnvironmentSoundMix, EnvironmentSoundClass, InVolume);
}

void AAudioModule::MultiSetEnvironmentSoundVolume_Implementation(float InVolume)
{
	SetEnvironmentSoundVolume(InVolume, false);
}

void AAudioModule::SetEffectSoundVolume(float InVolume, bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
		{
			MultiSetEffectSoundVolume(InVolume);
		}
		else if(UAudioModuleNetworkComponent* AudioModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UAudioModuleNetworkComponent>())
		{
			AudioModuleNetworkComponent->ServerSetEffectSoundVolumeMulticast(InVolume);
		}
		return;
	}
	SetSoundVolumeImpl(EffectSoundMix, EffectSoundClass, InVolume);
}

void AAudioModule::MultiSetEffectSoundVolume_Implementation(float InVolume)
{
	SetEffectSoundVolume(InVolume, false);
}

void AAudioModule::SetSoundVolumeImpl(USoundMix* InSoundMix, USoundClass* InSoundClass, float InVolume)
{
	if(InSoundMix && InSoundClass)
	{
		UGameplayStatics::SetSoundMixClassOverride(this, InSoundMix, InSoundClass, InVolume);
		UGameplayStatics::PushSoundMixModifier(this, InSoundMix);
	}
}

void AAudioModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AAudioModule, SingleSoundHandle);
}
