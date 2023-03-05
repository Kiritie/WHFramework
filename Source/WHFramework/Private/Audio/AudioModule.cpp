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
void AAudioModule::OnGenerate_Implementation()
{
	Super::OnGenerate_Implementation();
}

void AAudioModule::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();
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

void AAudioModule::OnTermination_Implementation()
{
	Super::OnTermination_Implementation();
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

void AAudioModule::PlaySingleSound2D(USoundBase* InSound, FName InFlag, float InVolume, bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
		{
			MultiPlaySingleSound2D(InSound, InFlag, InVolume);
		}
		else if(UAudioModuleNetworkComponent* AudioModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UAudioModuleNetworkComponent>())
		{
			AudioModuleNetworkComponent->ServerPlaySingleSound2DMulticast(InSound, InFlag, InVolume);
		}
		return;
	}
	NativePlaySingleSound(InFlag, UGameplayStatics::SpawnSound2D(this, InSound, InVolume));
}

void AAudioModule::MultiPlaySingleSound2D_Implementation(USoundBase* InSound, FName InFlag, float InVolume)
{
	PlaySingleSound2D(InSound, InFlag, InVolume, false);
}

void AAudioModule::PlaySingleSoundAtLocation(USoundBase* InSound, FName InFlag, FVector InLocation, float InVolume, bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
		{
			MultiPlaySingleSoundAtLocation(InSound, InFlag, InLocation, InVolume);
		}
		else if(UAudioModuleNetworkComponent* AudioModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UAudioModuleNetworkComponent>())
		{
			AudioModuleNetworkComponent->ServerPlaySingleSoundAtLocationMulticast(InSound, InFlag, InLocation, InVolume);
		}
		return;
	}
	NativePlaySingleSound(InFlag, UGameplayStatics::SpawnSoundAtLocation(this, InSound, InLocation, FRotator::ZeroRotator, InVolume));
}

void AAudioModule::MultiPlaySingleSoundAtLocation_Implementation(USoundBase* InSound, FName InFlag, FVector InLocation, float InVolume)
{
	PlaySingleSoundAtLocation(InSound, InFlag, InLocation, InVolume, false);
}

void AAudioModule::PlaySingleSound2DWithDelegate(USoundBase* InSound, FName InFlag, const FOnSoundPlayFinishDelegate& InOnSoundPlayFinish, float InVolume)
{
	if(UAudioComponent* AudioComponent = UGameplayStatics::SpawnSound2D(this, InSound, InVolume))
	{
		AudioComponent->OnAudioFinished.Add(InOnSoundPlayFinish);
		NativePlaySingleSound(InFlag, AudioComponent);
	}
}

void AAudioModule::PlaySingleSoundAtLocationWithDelegate(USoundBase* InSound, FName InFlag, const FOnSoundPlayFinishDelegate& InOnSoundPlayFinish, FVector InLocation, float InVolume)
{
	if(UAudioComponent* AudioComponent = UGameplayStatics::SpawnSoundAtLocation(this, InSound, InLocation, FRotator::ZeroRotator, InVolume))
	{
		AudioComponent->OnAudioFinished.Add(InOnSoundPlayFinish);
		NativePlaySingleSound(InFlag, AudioComponent);
	}
}

void AAudioModule::StopSingleSound(FName InFlag, bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
		{
			MultiStopSingleSound(InFlag);
		}
		else if(UAudioModuleNetworkComponent* AudioModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UAudioModuleNetworkComponent>())
		{
			AudioModuleNetworkComponent->ServerStopSingleSoundMulticast(InFlag);
		}
		return;
	}
	NativeStopSingleSound(InFlag);
}

void AAudioModule::MultiStopSingleSound_Implementation(FName InFlag)
{
	StopSingleSound(InFlag, false);
}

void AAudioModule::SetSingleSoundPaused(FName InFlag, bool bPaused, bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
		{
			MultiSetSingleSoundPaused(InFlag, bPaused);
		}
		else if(UAudioModuleNetworkComponent* AudioModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UAudioModuleNetworkComponent>())
		{
			AudioModuleNetworkComponent->ServerSetSingleSoundPausedMulticast(InFlag, bPaused);
		}
		return;
	}
	NativeSetSingleSoundPaused(InFlag, bPaused);
}

void AAudioModule::MultiSetSingleSoundPaused_Implementation(FName InFlag, bool bPaused)
{
	SetSingleSoundPaused(InFlag, bPaused);
}

void AAudioModule::NativePlaySingleSound(FName InFlag, UAudioComponent* InAudioComponent)
{
	NativeStopSingleSound(InFlag);
	if(InAudioComponent)
	{
		InAudioComponent->Play();
		AudioComponents.Add(InFlag, InAudioComponent);
	}
}

void AAudioModule::NativeStopSingleSound(FName InFlag)
{
	if(AudioComponents.Contains(InFlag))
	{
		if(IsValid(AudioComponents[InFlag]))
		{
			AudioComponents[InFlag]->Stop();
		}
		AudioComponents.Remove(InFlag);
	}
}

void AAudioModule::NativeSetSingleSoundPaused(FName InFlag, bool bPaused)
{
	if(AudioComponents.Contains(InFlag))
	{
		if(IsValid(AudioComponents[InFlag]))
		{
			AudioComponents[InFlag]->SetPaused(bPaused);
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
	NativeSetSoundVolume(GlobalSoundMix, GlobalSoundClass, InVolume);
}

void AAudioModule::MultiSetGlobalSoundVolume_Implementation(float InVolume)
{
	SetGlobalSoundVolume(InVolume, false);
}

void AAudioModule::SetBGMSoundVolume(float InVolume, bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
		{
			MultiSetBGMSoundVolume(InVolume);
		}
		else if(UAudioModuleNetworkComponent* AudioModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UAudioModuleNetworkComponent>())
		{
			AudioModuleNetworkComponent->ServerSetBGMSoundVolumeMulticast(InVolume);
		}
		return;
	}
	NativeSetSoundVolume(BGMSoundMix, BGMSoundClass, InVolume);
}

void AAudioModule::MultiSetBGMSoundVolume_Implementation(float InVolume)
{
	SetBGMSoundVolume(InVolume, false);
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
	NativeSetSoundVolume(EnvironmentSoundMix, EnvironmentSoundClass, InVolume);
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
	NativeSetSoundVolume(EffectSoundMix, EffectSoundClass, InVolume);
}

void AAudioModule::MultiSetEffectSoundVolume_Implementation(float InVolume)
{
	SetEffectSoundVolume(InVolume, false);
}

void AAudioModule::NativeSetSoundVolume(USoundMix* InSoundMix, USoundClass* InSoundClass, float InVolume)
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

}
