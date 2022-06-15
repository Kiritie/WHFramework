// Fill out your copyright notice in the Description page of Project Settings.


#include "Audio/AudioModule.h"

#include "Debug/DebugModuleTypes.h"
#include "Components/AudioComponent.h"
#include "Audio/AudioModuleBPLibrary.h"
#include "Audio/AudioModuleNetworkComponent.h"
#include "Main/MainModule.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"

// Sets default values
AAudioModule::AAudioModule()
{
	ModuleName = FName("AudioModule");
	
	SingleSoundComponent = CreateDefaultSubobject<UAudioComponent>(FName("SingleSoundComponent"));
	SingleSoundComponent->SetupAttachment(RootComponent);
	SingleSoundComponent->OnAudioFinished.AddDynamic(this, &AAudioModule::OnSingleSoundPlayFinished);

	bAutoPlayGlobal = true;

	GlobalBGSoundComponent = CreateDefaultSubobject<UAudioComponent>(FName("GlobalBGSoundComponent"));
	GlobalBGSoundComponent->SetupAttachment(RootComponent);

	SingleBGSoundComponent = CreateDefaultSubobject<UAudioComponent>(FName("SingleBGSoundComponent"));
	SingleBGSoundComponent->SetupAttachment(RootComponent);
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
	
	if(bAutoPlayGlobal)
	{
		PlayGlobalBGSound();
	}
}

void AAudioModule::OnPreparatory_Implementation()
{
	Super::OnPreparatory_Implementation();
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
	UGameplayStatics::PlaySound2D(this, InSound, SingleSoundInfo.VolumeScale * InVolume);
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
	UGameplayStatics::PlaySoundAtLocation(this, InSound, InLocation, SingleSoundInfo.VolumeScale * InVolume);
}

void AAudioModule::MultiPlaySoundAtLocation_Implementation(USoundBase* InSound, FVector InLocation, float InVolume)
{
	PlaySoundAtLocation(InSound, InLocation, InVolume, false);
}

void AAudioModule::PlaySingleSound2D(USoundBase* InSound, float InVolume, bool bMulticast)
{
	if(!InSound) return;

	StopSingleSound();
	
	if(bMulticast)
	{
		SingleSoundInfo = FSingleSoundInfo(InSound, InVolume, true);
		OnRep_SingleSoundInfo();
	}
	else
	{
		SingleSoundComponent->SetSound(InSound);
		SingleSoundComponent->SetVolumeMultiplier(InVolume);
		SingleSoundComponent->Play();
		GetWorld()->GetTimerManager().SetTimer(SingleSoundStopTimerHandle, this, &AAudioModule::StopSingleSound, InSound->GetDuration());
	}
}

void AAudioModule::PlaySingleSound2DWithDelegate(USoundBase* InSound, const FOnSoundPlayFinishedDelegate& InOnSoundPlayFinished, float InVolume, bool bMulticast)
{
	PlaySingleSound2D(InSound, InVolume, bMulticast);
	OnSingleSoundPlayFinishedDelegate = InOnSoundPlayFinished;
}

void AAudioModule::PlaySingleSoundAtLocation(USoundBase* InSound, FVector InLocation, float InVolume, bool bMulticast)
{
	if(!InSound) return;
	
	StopSingleSound();

	if(bMulticast)
	{
		SingleSoundInfo = FSingleSoundInfo(InSound, InVolume, false, InLocation);
		OnRep_SingleSoundInfo();
	}
	else
	{
		SingleSoundComponent->SetSound(InSound);
		SingleSoundComponent->SetVolumeMultiplier(InVolume);
		SingleSoundComponent->SetWorldLocation(InLocation);
		SingleSoundComponent->Play();
		GetWorld()->GetTimerManager().SetTimer(SingleSoundStopTimerHandle, this, &AAudioModule::StopSingleSound, InSound->GetDuration());
	}
}

void AAudioModule::PlaySingleSoundAtLocationWithDelegate(USoundBase* InSound, const FOnSoundPlayFinishedDelegate& InOnSoundPlayFinished, FVector InLocation, float InVolume, bool bMulticast)
{
	PlaySingleSoundAtLocation(InSound, InLocation, InVolume, bMulticast);
	OnSingleSoundPlayFinishedDelegate = InOnSoundPlayFinished;
}

void AAudioModule::StopSingleSound()
{
	SingleSoundInfo.Sound = nullptr;
	OnRep_SingleSoundInfo();
	OnSingleSoundPlayFinished();
	GetWorld()->GetTimerManager().ClearTimer(SingleSoundStopTimerHandle);
}

float AAudioModule::GetSingleSoundVolume()
{
	return SingleSoundInfo.VolumeScale;
}

void AAudioModule::SetSingleSoundVolume(float InVolume)
{
	SingleSoundInfo.VolumeScale = InVolume;
	SingleSoundComponent->VolumeMultiplier = SingleSoundInfo.Volume * InVolume;
}

void AAudioModule::OnRep_SingleSoundInfo()
{
	if(!SingleSoundInfo.Sound)
	{
		SingleSoundComponent->Stop();
		return;
	}
	
	if(SingleSoundComponent->Sound != SingleSoundInfo.Sound)
	{
		SingleSoundComponent->Sound = SingleSoundInfo.Sound;
	}
	SingleSoundComponent->VolumeMultiplier = SingleSoundInfo.Volume * SingleSoundInfo.VolumeScale;
	SingleSoundComponent->bIsUISound = SingleSoundInfo.bUISound;
	if(!SingleSoundInfo.bUISound)
	{
		SingleSoundComponent->SetWorldLocation(SingleSoundInfo.Location);
	}
	SingleSoundComponent->Play();

	if(!HasAuthority())
	{
		GetWorld()->GetTimerManager().SetTimer(SingleSoundStopTimerHandle, this, &AAudioModule::StopSingleSound, SingleSoundInfo.Sound->GetDuration());
	}
}

void AAudioModule::OnSingleSoundPlayFinished()
{
	if(OnSingleSoundPlayFinishedDelegate.IsBound())
	{
		OnSingleSoundPlayFinishedDelegate.Execute();
		OnSingleSoundPlayFinishedDelegate.Clear();
	}
}

void AAudioModule::PlayGlobalBGSound()
{
	if(!GlobalBGSoundInfo.Sound) return;
	
	if(GlobalBGSoundInfo.bLoopSound && !GlobalBGSoundInfo.Sound->IsLooping())
	{
		if(GlobalBGSoundInfo.SoundState == ESoundState::Pausing)
		{
			GetWorld()->GetTimerManager().UnPauseTimer(GlobalBGSoundLoopTimerHandle);
		}
		else
		{
			GetWorld()->GetTimerManager().SetTimer(GlobalBGSoundLoopTimerHandle, this, &AAudioModule::OnLoopGlobalBGSound, FMath::Max(GlobalBGSoundInfo.Sound->GetDuration() - 1.f, 0.f));
		}
	}
	GlobalBGSoundInfo.SoundState = ESoundState::Playing;
	OnRep_GlobalBGSoundInfo();
}

void AAudioModule::PauseGlobalBGSound()
{
	GetWorld()->GetTimerManager().PauseTimer(SingleBGSoundLoopTimerHandle);
	GlobalBGSoundInfo.SoundState = ESoundState::Pausing;
	OnRep_GlobalBGSoundInfo();
	GetWorld()->GetTimerManager().PauseTimer(GlobalBGSoundLoopTimerHandle);
}

void AAudioModule::StopGlobalBGSound()
{
	GlobalBGSoundInfo.SoundState = ESoundState::Stopped;
	OnRep_GlobalBGSoundInfo();
	GetWorld()->GetTimerManager().ClearTimer(GlobalBGSoundLoopTimerHandle);
}

float AAudioModule::GetGlobalBGSoundVolume()
{
	return GlobalBGSoundInfo.VolumeScale;
}

void AAudioModule::SetGlobalBGSoundVolume(float InVolume)
{
	GlobalBGSoundInfo.VolumeScale = InVolume;
	GlobalBGSoundComponent->VolumeMultiplier = GlobalBGSoundInfo.Volume * InVolume;
}

void AAudioModule::OnLoopGlobalBGSound()
{
	GlobalBGSoundInfo.SoundState = ESoundState::None;
	GetWorld()->GetTimerManager().SetTimer(GlobalBGSoundLoopTimerHandle, this, &AAudioModule::PlayGlobalBGSound, 1.f);
}

void AAudioModule::OnRep_GlobalBGSoundInfo()
{
	if(!GlobalBGSoundInfo.Sound) return;
	
	if(GlobalBGSoundComponent->Sound != GlobalBGSoundInfo.Sound)
	{
		GlobalBGSoundComponent->Sound = GlobalBGSoundInfo.Sound;
	}
	GlobalBGSoundComponent->VolumeMultiplier = GlobalBGSoundInfo.Volume * GlobalBGSoundInfo.VolumeScale;
	GlobalBGSoundComponent->bIsUISound = GlobalBGSoundInfo.bUISound;

	switch(GlobalBGSoundInfo.SoundState)
	{
		case ESoundState::Playing:
		{
			if(GlobalBGSoundComponent->bIsPaused)
			{
				GlobalBGSoundComponent->SetPaused(false);
			}
			else
			{
				GlobalBGSoundComponent->Play();
			}
			break;
		}
		case ESoundState::Pausing:
		{
			GlobalBGSoundComponent->SetPaused(true);
			break;
		}
		case ESoundState::Stopped:
		{
			GlobalBGSoundComponent->Stop();
			break;
		}
		default: { }
	}
}

void AAudioModule::InitSingleBGSound(USoundBase* InBGSound, float InBGVolume, bool bIsLoopSound, bool bIsUISound, bool bIsAutoPlay)
{
	SingleBGSoundInfo = FBGSoundInfo(InBGSound, ESoundState::None, InBGVolume, bIsLoopSound, bIsUISound);
	OnRep_SingleBGSoundInfo();
	if(bIsAutoPlay)
	{
		PlaySingleBGSound();
	}
}

void AAudioModule::PlaySingleBGSound()
{
	if(!SingleBGSoundInfo.Sound) return;

	PauseGlobalBGSound();
	
	if(SingleBGSoundInfo.bLoopSound && !SingleBGSoundInfo.Sound->IsLooping())
	{
		if(SingleBGSoundInfo.SoundState == ESoundState::Pausing)
		{
			GetWorld()->GetTimerManager().UnPauseTimer(SingleBGSoundLoopTimerHandle);
		}
		else
		{
			GetWorld()->GetTimerManager().SetTimer(SingleBGSoundLoopTimerHandle, this, &AAudioModule::OnLoopSingleBGSound, FMath::Max(SingleBGSoundInfo.Sound->GetDuration() - 1.f, 0.f));
		}
	}
	SingleBGSoundInfo.SoundState = ESoundState::Playing;
	OnRep_SingleBGSoundInfo();
}

void AAudioModule::PauseSingleBGSound()
{
	SingleBGSoundInfo.SoundState = ESoundState::Pausing;
	OnRep_SingleBGSoundInfo();
	GetWorld()->GetTimerManager().PauseTimer(SingleBGSoundLoopTimerHandle);
}

void AAudioModule::StopSingleBGSound()
{
	if(bAutoPlayGlobal)
	{
		PlayGlobalBGSound();
	}
	SingleBGSoundInfo.SoundState = ESoundState::Stopped;
	OnRep_SingleBGSoundInfo();
	GetWorld()->GetTimerManager().ClearTimer(SingleBGSoundLoopTimerHandle);
}

float AAudioModule::GetSingleBGSoundVolume()
{
	return SingleBGSoundInfo.VolumeScale;
}

void AAudioModule::SetSingleBGSoundVolume(float InVolume)
{
	SingleBGSoundInfo.VolumeScale = InVolume;
	SingleBGSoundComponent->VolumeMultiplier = SingleBGSoundInfo.Volume * InVolume;
}

void AAudioModule::OnLoopSingleBGSound()
{
	SingleBGSoundInfo.SoundState = ESoundState::None;
	GetWorld()->GetTimerManager().SetTimer(SingleBGSoundLoopTimerHandle, this, &AAudioModule::PlaySingleBGSound, 1.f);
}

void AAudioModule::OnRep_SingleBGSoundInfo()
{
	if(!SingleBGSoundInfo.Sound) return;
	
	if(SingleBGSoundComponent->Sound != SingleBGSoundInfo.Sound)
	{
		SingleBGSoundComponent->Sound = SingleBGSoundInfo.Sound;
	}
	SingleBGSoundComponent->VolumeMultiplier = SingleBGSoundInfo.Volume * SingleBGSoundInfo.VolumeScale;
	SingleBGSoundComponent->bIsUISound = SingleBGSoundInfo.bUISound;

	switch(SingleBGSoundInfo.SoundState)
	{
		case ESoundState::Playing:
		{
			if(SingleBGSoundComponent->bIsPaused)
			{
				SingleBGSoundComponent->SetPaused(false);
			}
			else
			{
				SingleBGSoundComponent->Play();
			}
			break;
		}
		case ESoundState::Pausing:
		{
			SingleBGSoundComponent->SetPaused(true);
			break;
		}
		case ESoundState::Stopped:
		{
			SingleBGSoundComponent->Stop();
			break;
		}
		default: { }
	}
}

void AAudioModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AAudioModule, GlobalBGSoundInfo);
	DOREPLIFETIME(AAudioModule, SingleBGSoundInfo);
}
