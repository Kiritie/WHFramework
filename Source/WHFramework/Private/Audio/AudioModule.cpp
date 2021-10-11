// Fill out your copyright notice in the Description page of Project Settings.


#include "Audio/AudioModule.h"

#include "Debug/DebugModuleTypes.h"
#include "Components/AudioComponent.h"
#include "Audio/AudioModuleBPLibrary.h"
#include "Audio/AudioModuleNetworkComponent.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"

// Sets default values
AAudioModule::AAudioModule()
{
	ModuleName = FName("AudioModule");

	bAutoPlayGlobalBGSound = true;
	
	GlobalBGSoundComponent = CreateDefaultSubobject<UAudioComponent>(FName("GlobalBGSoundComponent"));
	GlobalBGSoundComponent->SetupAttachment(RootComponent);

	GameBGSoundComponent = CreateDefaultSubobject<UAudioComponent>(FName("GameBGSoundComponent"));
	GameBGSoundComponent->SetupAttachment(RootComponent);

	SingleSoundComponent = CreateDefaultSubobject<UAudioComponent>(FName("SingleSoundComponent"));
	SingleSoundComponent->SetupAttachment(RootComponent);
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
	
	if(bAutoPlayGlobalBGSound)
	{
		PlayGlobalBGSound();
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

void AAudioModule::PlaySingleSound2D(USoundBase* InSound, float InVolume, bool bMulticast)
{
	if(!InSound) return;

	StopSingleSound();
	
	if(bMulticast)
	{
		SingleSoundParams = FSingleSoundParams(InSound, InVolume, true);
		OnRep_SingleSoundParams();
	}
	else
	{
		SingleSoundComponent->SetSound(InSound);
		SingleSoundComponent->SetVolumeMultiplier(InVolume);
		SingleSoundComponent->Play();
		GetWorld()->GetTimerManager().SetTimer(SingleSoundStopTimerHandle, this, &AAudioModule::StopSingleSound, InSound->GetDuration());
	}
}

void AAudioModule::PlaySingleSoundAtLocation(USoundBase* InSound, FVector InLocation, float InVolume, bool bMulticast)
{
	if(!InSound) return;
	
	StopSingleSound();

	if(bMulticast)
	{
		SingleSoundParams = FSingleSoundParams(InSound, InVolume, false, InLocation);
		OnRep_SingleSoundParams();
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

void AAudioModule::StopSingleSound()
{
	SingleSoundParams.Sound = nullptr;
	OnRep_SingleSoundParams();
	GetWorld()->GetTimerManager().ClearTimer(SingleSoundStopTimerHandle);
}

void AAudioModule::OnRep_SingleSoundParams()
{
	if(!SingleSoundParams.Sound)
	{
		SingleSoundComponent->Stop();
		return;
	}
	
	if(SingleSoundComponent->Sound != SingleSoundParams.Sound)
	{
		SingleSoundComponent->Sound = SingleSoundParams.Sound;
	}
	SingleSoundComponent->VolumeMultiplier = SingleSoundParams.Volume;
	SingleSoundComponent->bIsUISound = SingleSoundParams.bIsUISound;
	if(!SingleSoundParams.bIsUISound)
	{
		SingleSoundComponent->SetWorldLocation(SingleSoundParams.Location);
	}
	SingleSoundComponent->Play();

	if(!HasAuthority())
	{
		GetWorld()->GetTimerManager().SetTimer(SingleSoundStopTimerHandle, this, &AAudioModule::StopSingleSound, SingleSoundParams.Sound->GetDuration());
	}
}

void AAudioModule::PlayGlobalBGSound()
{
	if(!GlobalBGSoundParams.BGSound) return;
	
	if(GlobalBGSoundParams.bIsLoopSound && !GlobalBGSoundParams.BGSound->IsLooping())
	{
		if(GlobalBGSoundParams.BGSoundState == EBGSoundState::Pausing)
		{
			GetWorld()->GetTimerManager().UnPauseTimer(GlobalBGSoundLoopTimerHandle);
		}
		else
		{
			GetWorld()->GetTimerManager().SetTimer(GlobalBGSoundLoopTimerHandle, this, &AAudioModule::OnLoopGlobalBGSound, FMath::Max(GlobalBGSoundParams.BGSound->GetDuration() - 1.f, 0.f));
		}
	}
	GlobalBGSoundParams.BGSoundState = EBGSoundState::Playing;
	OnRep_GlobalBGSoundParams();
}

void AAudioModule::PauseGlobalBGSound()
{
	GetWorld()->GetTimerManager().PauseTimer(GameBGSoundLoopTimerHandle);
	GlobalBGSoundParams.BGSoundState = EBGSoundState::Pausing;
	OnRep_GlobalBGSoundParams();
	GetWorld()->GetTimerManager().PauseTimer(GlobalBGSoundLoopTimerHandle);
}

void AAudioModule::StopGlobalBGSound()
{
	GlobalBGSoundParams.BGSoundState = EBGSoundState::Stopped;
	OnRep_GlobalBGSoundParams();
	GetWorld()->GetTimerManager().ClearTimer(GlobalBGSoundLoopTimerHandle);
}

void AAudioModule::OnLoopGlobalBGSound()
{
	GlobalBGSoundParams.BGSoundState = EBGSoundState::None;
	GetWorld()->GetTimerManager().SetTimer(GlobalBGSoundLoopTimerHandle, this, &AAudioModule::PlayGlobalBGSound, 1.f);
}

void AAudioModule::OnRep_GlobalBGSoundParams()
{
	if(!GlobalBGSoundParams.BGSound) return;
	
	if(GlobalBGSoundComponent->Sound != GlobalBGSoundParams.BGSound)
	{
		GlobalBGSoundComponent->Sound = GlobalBGSoundParams.BGSound;
	}
	GlobalBGSoundComponent->VolumeMultiplier = GlobalBGSoundParams.BGVolume;
	GlobalBGSoundComponent->bIsUISound = GlobalBGSoundParams.bIsUISound;

	switch(GlobalBGSoundParams.BGSoundState)
	{
		case EBGSoundState::Playing:
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
		case EBGSoundState::Pausing:
		{
			GlobalBGSoundComponent->SetPaused(true);
			break;
		}
		case EBGSoundState::Stopped:
		{
			GlobalBGSoundComponent->Stop();
			break;
		}
		default: { }
	}
}

void AAudioModule::InitGameBGSound(USoundBase* InBGSound, float InBGVolume, bool bIsLoopSound, bool bIsUISound, bool bIsAutoPlay)
{
	GameBGSoundParams = FBGSoundParams(InBGSound, EBGSoundState::None, InBGVolume, bIsLoopSound, bIsUISound);
	OnRep_GameBGSoundParams();
	if(bIsAutoPlay)
	{
		PlayGameBGSound();
	}
}

void AAudioModule::PlayGameBGSound()
{
	if(!GameBGSoundParams.BGSound) return;

	PauseGlobalBGSound();
	
	if(GameBGSoundParams.bIsLoopSound && !GameBGSoundParams.BGSound->IsLooping())
	{
		if(GameBGSoundParams.BGSoundState == EBGSoundState::Pausing)
		{
			GetWorld()->GetTimerManager().UnPauseTimer(GameBGSoundLoopTimerHandle);
		}
		else
		{
			GetWorld()->GetTimerManager().SetTimer(GameBGSoundLoopTimerHandle, this, &AAudioModule::OnLoopGameBGSound, FMath::Max(GameBGSoundParams.BGSound->GetDuration() - 1.f, 0.f));
		}
	}
	GameBGSoundParams.BGSoundState = EBGSoundState::Playing;
	OnRep_GameBGSoundParams();
}

void AAudioModule::PauseGameBGSound()
{
	GameBGSoundParams.BGSoundState = EBGSoundState::Pausing;
	OnRep_GameBGSoundParams();
	GetWorld()->GetTimerManager().PauseTimer(GameBGSoundLoopTimerHandle);
}

void AAudioModule::StopGameBGSound()
{
	if(bAutoPlayGlobalBGSound)
	{
		PlayGlobalBGSound();
	}
	GameBGSoundParams.BGSoundState = EBGSoundState::Stopped;
	OnRep_GameBGSoundParams();
	GetWorld()->GetTimerManager().ClearTimer(GameBGSoundLoopTimerHandle);
}

void AAudioModule::OnLoopGameBGSound()
{
	GameBGSoundParams.BGSoundState = EBGSoundState::None;
	GetWorld()->GetTimerManager().SetTimer(GameBGSoundLoopTimerHandle, this, &AAudioModule::PlayGameBGSound, 1.f);
}

void AAudioModule::OnRep_GameBGSoundParams()
{
	if(!GameBGSoundParams.BGSound) return;
	
	if(GameBGSoundComponent->Sound != GameBGSoundParams.BGSound)
	{
		GameBGSoundComponent->Sound = GameBGSoundParams.BGSound;
	}
	GameBGSoundComponent->VolumeMultiplier = GameBGSoundParams.BGVolume;
	GameBGSoundComponent->bIsUISound = GameBGSoundParams.bIsUISound;

	switch(GameBGSoundParams.BGSoundState)
	{
		case EBGSoundState::Playing:
		{
			if(GameBGSoundComponent->bIsPaused)
			{
				GameBGSoundComponent->SetPaused(false);
			}
			else
			{
				GameBGSoundComponent->Play();
			}
			break;
		}
		case EBGSoundState::Pausing:
		{
			GameBGSoundComponent->SetPaused(true);
			break;
		}
		case EBGSoundState::Stopped:
		{
			GameBGSoundComponent->Stop();
			break;
		}
		default: { }
	}
}

void AAudioModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AAudioModule, GlobalBGSoundParams);
	DOREPLIFETIME(AAudioModule, GameBGSoundParams);
}
