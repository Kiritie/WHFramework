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

	bAutoPlayGlobalBGSound = true;

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
	
	if(bAutoPlayGlobalBGSound)
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
	UGameplayStatics::PlaySoundAtLocation(this, InSound, InLocation,InVolume);
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
	GetWorld()->GetTimerManager().PauseTimer(SingleBGSoundLoopTimerHandle);
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

void AAudioModule::InitSingleBGSound(USoundBase* InBGSound, float InBGVolume, bool bIsLoopSound, bool bIsUISound, bool bIsAutoPlay)
{
	SingleBGSoundParams = FBGSoundParams(InBGSound, EBGSoundState::None, InBGVolume, bIsLoopSound, bIsUISound);
	OnRep_SingleBGSoundParams();
	if(bIsAutoPlay)
	{
		PlaySingleBGSound();
	}
}

void AAudioModule::PlaySingleBGSound()
{
	if(!SingleBGSoundParams.BGSound) return;

	PauseGlobalBGSound();
	
	if(SingleBGSoundParams.bIsLoopSound && !SingleBGSoundParams.BGSound->IsLooping())
	{
		if(SingleBGSoundParams.BGSoundState == EBGSoundState::Pausing)
		{
			GetWorld()->GetTimerManager().UnPauseTimer(SingleBGSoundLoopTimerHandle);
		}
		else
		{
			GetWorld()->GetTimerManager().SetTimer(SingleBGSoundLoopTimerHandle, this, &AAudioModule::OnLoopSingleBGSound, FMath::Max(SingleBGSoundParams.BGSound->GetDuration() - 1.f, 0.f));
		}
	}
	SingleBGSoundParams.BGSoundState = EBGSoundState::Playing;
	OnRep_SingleBGSoundParams();
}

void AAudioModule::PauseSingleBGSound()
{
	SingleBGSoundParams.BGSoundState = EBGSoundState::Pausing;
	OnRep_SingleBGSoundParams();
	GetWorld()->GetTimerManager().PauseTimer(SingleBGSoundLoopTimerHandle);
}

void AAudioModule::StopSingleBGSound()
{
	if(bAutoPlayGlobalBGSound)
	{
		PlayGlobalBGSound();
	}
	SingleBGSoundParams.BGSoundState = EBGSoundState::Stopped;
	OnRep_SingleBGSoundParams();
	GetWorld()->GetTimerManager().ClearTimer(SingleBGSoundLoopTimerHandle);
}

void AAudioModule::OnLoopSingleBGSound()
{
	SingleBGSoundParams.BGSoundState = EBGSoundState::None;
	GetWorld()->GetTimerManager().SetTimer(SingleBGSoundLoopTimerHandle, this, &AAudioModule::PlaySingleBGSound, 1.f);
}

void AAudioModule::OnRep_SingleBGSoundParams()
{
	if(!SingleBGSoundParams.BGSound) return;
	
	if(SingleBGSoundComponent->Sound != SingleBGSoundParams.BGSound)
	{
		SingleBGSoundComponent->Sound = SingleBGSoundParams.BGSound;
	}
	SingleBGSoundComponent->VolumeMultiplier = SingleBGSoundParams.BGVolume;
	SingleBGSoundComponent->bIsUISound = SingleBGSoundParams.bIsUISound;

	switch(SingleBGSoundParams.BGSoundState)
	{
		case EBGSoundState::Playing:
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
		case EBGSoundState::Pausing:
		{
			SingleBGSoundComponent->SetPaused(true);
			break;
		}
		case EBGSoundState::Stopped:
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

	DOREPLIFETIME(AAudioModule, GlobalBGSoundParams);
	DOREPLIFETIME(AAudioModule, SingleBGSoundParams);
}
