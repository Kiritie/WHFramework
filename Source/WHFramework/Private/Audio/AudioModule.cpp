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

void AAudioModule::PlaySingleSound2DWithDelegate(USoundBase* InSound, const FOnSoundPlayFinishedDelegate& InOnSoundPlayFinishedDelegate, float InVolume, bool bMulticast)
{
	PlaySingleSound2D(InSound, InVolume, bMulticast);
	OnSoundPlayFinishedDelegate = InOnSoundPlayFinishedDelegate;
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

void AAudioModule::StopSingleSound()
{
	SingleSoundInfo.Sound = nullptr;
	OnRep_SingleSoundInfo();
	OnSingleSoundPlayFinished();
	GetWorld()->GetTimerManager().ClearTimer(SingleSoundStopTimerHandle);
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
	SingleSoundComponent->VolumeMultiplier = SingleSoundInfo.Volume;
	SingleSoundComponent->bIsUISound = SingleSoundInfo.bIsUISound;
	if(!SingleSoundInfo.bIsUISound)
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
	if(OnSoundPlayFinishedDelegate.IsBound())
	{
		OnSoundPlayFinishedDelegate.Execute();
		OnSoundPlayFinishedDelegate.Clear();
	}
}

void AAudioModule::PlayGlobalBGSound()
{
	if(!GlobalBGSoundInfo.BGSound) return;
	
	if(GlobalBGSoundInfo.bIsLoopSound && !GlobalBGSoundInfo.BGSound->IsLooping())
	{
		if(GlobalBGSoundInfo.BGSoundState == EBGSoundState::Pausing)
		{
			GetWorld()->GetTimerManager().UnPauseTimer(GlobalBGSoundLoopTimerHandle);
		}
		else
		{
			GetWorld()->GetTimerManager().SetTimer(GlobalBGSoundLoopTimerHandle, this, &AAudioModule::OnLoopGlobalBGSound, FMath::Max(GlobalBGSoundInfo.BGSound->GetDuration() - 1.f, 0.f));
		}
	}
	GlobalBGSoundInfo.BGSoundState = EBGSoundState::Playing;
	OnRep_GlobalBGSoundInfo();
}

void AAudioModule::PauseGlobalBGSound()
{
	GetWorld()->GetTimerManager().PauseTimer(SingleBGSoundLoopTimerHandle);
	GlobalBGSoundInfo.BGSoundState = EBGSoundState::Pausing;
	OnRep_GlobalBGSoundInfo();
	GetWorld()->GetTimerManager().PauseTimer(GlobalBGSoundLoopTimerHandle);
}

void AAudioModule::StopGlobalBGSound()
{
	GlobalBGSoundInfo.BGSoundState = EBGSoundState::Stopped;
	OnRep_GlobalBGSoundInfo();
	GetWorld()->GetTimerManager().ClearTimer(GlobalBGSoundLoopTimerHandle);
}

void AAudioModule::OnLoopGlobalBGSound()
{
	GlobalBGSoundInfo.BGSoundState = EBGSoundState::None;
	GetWorld()->GetTimerManager().SetTimer(GlobalBGSoundLoopTimerHandle, this, &AAudioModule::PlayGlobalBGSound, 1.f);
}

void AAudioModule::OnRep_GlobalBGSoundInfo()
{
	if(!GlobalBGSoundInfo.BGSound) return;
	
	if(GlobalBGSoundComponent->Sound != GlobalBGSoundInfo.BGSound)
	{
		GlobalBGSoundComponent->Sound = GlobalBGSoundInfo.BGSound;
	}
	GlobalBGSoundComponent->VolumeMultiplier = GlobalBGSoundInfo.BGVolume;
	GlobalBGSoundComponent->bIsUISound = GlobalBGSoundInfo.bIsUISound;

	switch(GlobalBGSoundInfo.BGSoundState)
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
	SingleBGSoundInfo = FBGSoundInfo(InBGSound, EBGSoundState::None, InBGVolume, bIsLoopSound, bIsUISound);
	OnRep_SingleBGSoundInfo();
	if(bIsAutoPlay)
	{
		PlaySingleBGSound();
	}
}

void AAudioModule::PlaySingleBGSound()
{
	if(!SingleBGSoundInfo.BGSound) return;

	PauseGlobalBGSound();
	
	if(SingleBGSoundInfo.bIsLoopSound && !SingleBGSoundInfo.BGSound->IsLooping())
	{
		if(SingleBGSoundInfo.BGSoundState == EBGSoundState::Pausing)
		{
			GetWorld()->GetTimerManager().UnPauseTimer(SingleBGSoundLoopTimerHandle);
		}
		else
		{
			GetWorld()->GetTimerManager().SetTimer(SingleBGSoundLoopTimerHandle, this, &AAudioModule::OnLoopSingleBGSound, FMath::Max(SingleBGSoundInfo.BGSound->GetDuration() - 1.f, 0.f));
		}
	}
	SingleBGSoundInfo.BGSoundState = EBGSoundState::Playing;
	OnRep_SingleBGSoundInfo();
}

void AAudioModule::PauseSingleBGSound()
{
	SingleBGSoundInfo.BGSoundState = EBGSoundState::Pausing;
	OnRep_SingleBGSoundInfo();
	GetWorld()->GetTimerManager().PauseTimer(SingleBGSoundLoopTimerHandle);
}

void AAudioModule::StopSingleBGSound()
{
	if(bAutoPlayGlobal)
	{
		PlayGlobalBGSound();
	}
	SingleBGSoundInfo.BGSoundState = EBGSoundState::Stopped;
	OnRep_SingleBGSoundInfo();
	GetWorld()->GetTimerManager().ClearTimer(SingleBGSoundLoopTimerHandle);
}

void AAudioModule::OnLoopSingleBGSound()
{
	SingleBGSoundInfo.BGSoundState = EBGSoundState::None;
	GetWorld()->GetTimerManager().SetTimer(SingleBGSoundLoopTimerHandle, this, &AAudioModule::PlaySingleBGSound, 1.f);
}

void AAudioModule::OnRep_SingleBGSoundInfo()
{
	if(!SingleBGSoundInfo.BGSound) return;
	
	if(SingleBGSoundComponent->Sound != SingleBGSoundInfo.BGSound)
	{
		SingleBGSoundComponent->Sound = SingleBGSoundInfo.BGSound;
	}
	SingleBGSoundComponent->VolumeMultiplier = SingleBGSoundInfo.BGVolume;
	SingleBGSoundComponent->bIsUISound = SingleBGSoundInfo.bIsUISound;

	switch(SingleBGSoundInfo.BGSoundState)
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

	DOREPLIFETIME(AAudioModule, GlobalBGSoundInfo);
	DOREPLIFETIME(AAudioModule, SingleBGSoundInfo);
}
