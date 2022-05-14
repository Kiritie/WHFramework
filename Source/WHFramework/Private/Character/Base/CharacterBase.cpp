// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Base/CharacterBase.h"

#include "AIController.h"
#include "Main/MainModule.h"
#include "Animation/AnimInstance.h"
#include "Components/AudioComponent.h"
#include "Debug/DebugModuleTypes.h"
#include "Asset/AssetModuleBPLibrary.h"
#include "Audio/AudioModuleBPLibrary.h"
#include "Character/CharacterModuleBPLibrary.h"
#include "Character/CharacterModuleNetworkComponent.h"
#include "Character/Base/CharacterAnim.h"
#include "Scene/SceneModuleBPLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Tasks/AITask_MoveTo.h"

ACharacterBase::ACharacterBase()
{
	bReplicates = true;

	Name = NAME_None;
	Anim = nullptr;

	SingleSound = CreateDefaultSubobject<UAudioComponent>(FName("SingleSound"));
	SingleSound->SetupAttachment(RootComponent);
}

void ACharacterBase::BeginPlay()
{
	Super::BeginPlay();

	USceneModuleBPLibrary::AddSceneActor(this);

	Anim = Cast<UCharacterAnim>(GetMesh()->GetAnimInstance());
}

void ACharacterBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

void ACharacterBase::OnRep_SingleSoundParams()
{
	if(!SingleSoundParams.Sound)
	{
		SingleSound->Stop();
		return;
	}
	
	if(SingleSound->Sound != SingleSoundParams.Sound)
	{
		SingleSound->Sound = SingleSoundParams.Sound;
	}
	SingleSound->VolumeMultiplier = SingleSoundParams.Volume;
	SingleSound->bIsUISound = SingleSoundParams.bIsUISound;
	if(!SingleSoundParams.bIsUISound)
	{
		SingleSound->SetWorldLocation(SingleSoundParams.Location);
	}
	SingleSound->Play();

	if(!HasAuthority())
	{
		GetWorld()->GetTimerManager().SetTimer(SingleSoundStopTimerHandle, this, &ACharacterBase::StopSound, SingleSoundParams.Sound->GetDuration());
	}
}

void ACharacterBase::PlaySound(USoundBase* InSound, float InVolume, bool bMulticast)
{
	if(!InSound) return;

	StopSound();

	if(bMulticast && !HasAuthority())
	{
		SingleSoundParams = FSingleSoundParams(InSound, InVolume, true);
		OnRep_SingleSoundParams();
	}
	else
	{
		SingleSound->SetSound(InSound);
		SingleSound->SetVolumeMultiplier(InVolume);
		SingleSound->Play();
		GetWorld()->GetTimerManager().SetTimer(SingleSoundStopTimerHandle, this, &ACharacterBase::StopSound, InSound->GetDuration());
	}
}

void ACharacterBase::StopSound()
{
	SingleSoundParams.Sound = nullptr;
	OnRep_SingleSoundParams();
	GetWorld()->GetTimerManager().ClearTimer(SingleSoundStopTimerHandle);
}

void ACharacterBase::PlayMontage(UAnimMontage* InMontage, bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
		{
			MultiPlayMontage(InMontage);
		}
		else if(UCharacterModuleNetworkComponent* CharacterModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UCharacterModuleNetworkComponent>())
		{
			CharacterModuleNetworkComponent->ServerPlayMontageMulticast(this, InMontage);
		}
		return;
	}
	if(Anim)
	{
		Anim->Montage_Play(InMontage);
	}
}

void ACharacterBase::MultiPlayMontage_Implementation(UAnimMontage* InMontage)
{
	PlayMontage(InMontage, false);
}

void ACharacterBase::PlayMontageByName(const FName InMontageName, bool bMulticast)
{
	
}

void ACharacterBase::StopMontage(UAnimMontage* InMontage, bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
		{
			MultiStopMontage(InMontage);
		}
		else if(UCharacterModuleNetworkComponent* CharacterModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UCharacterModuleNetworkComponent>())
		{
			CharacterModuleNetworkComponent->ServerStopMontageMulticast(this, InMontage);
		}
		return;
	}
	if(Anim)
	{
		Anim->Montage_Stop(.2f, InMontage);
	}
}

void ACharacterBase::MultiStopMontage_Implementation(UAnimMontage* InMontage)
{
	StopMontage(InMontage, false);
}

void ACharacterBase::StopMontageByName(const FName InMontageName, bool bMulticast)
{
	
}

void ACharacterBase::TransformTowards(FTransform InTransform, float InDuration, bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
		{
			MultiTransformTowards(InTransform);
		}
		else if(UCharacterModuleNetworkComponent* CharacterModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UCharacterModuleNetworkComponent>())
		{
			CharacterModuleNetworkComponent->ServerTransformTowardsToMulticast(this, InTransform);
		}
		return;
	}
	SetActorTransform(InTransform);
}

void ACharacterBase::MultiTransformTowards_Implementation(FTransform InTransform, float InDuration)
{
	TransformTowards(InTransform, false);
}

void ACharacterBase::RotationTowards(FRotator InRotation, float InDuration, bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
		{
			MultiRotationTowards(InRotation, InDuration);
		}
		else if(UCharacterModuleNetworkComponent* CharacterModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UCharacterModuleNetworkComponent>())
		{
			CharacterModuleNetworkComponent->ServerRotationTowardsMulticast(this, InRotation, InDuration);
		}
		return;
	}
}

void ACharacterBase::MultiRotationTowards_Implementation(FRotator InRotation, float InDuration)
{
	RotationTowards(InRotation, InDuration, false);
}

void ACharacterBase::AIMoveTo(FVector InLocation, float InStopDistance, bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
		{
			MultiAIMoveTo(InLocation, InStopDistance);
		}
		else if(UCharacterModuleNetworkComponent* CharacterModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UCharacterModuleNetworkComponent>())
		{
			CharacterModuleNetworkComponent->ServerAIMoveToMulticast(this, InLocation, InStopDistance);
		}
		return;
	}
	UAITask_MoveTo::AIMoveTo(Cast<AAIController>(GetController()), InLocation);
}

void ACharacterBase::MultiAIMoveTo_Implementation(FVector InLocation, float InStopDistance)
{
	AIMoveTo(InLocation, InStopDistance, false);
}

void ACharacterBase::StopAIMove(bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
		{
			MultiStopAIMove();
		}
		else if(UCharacterModuleNetworkComponent* CharacterModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UCharacterModuleNetworkComponent>())
		{
			CharacterModuleNetworkComponent->ServerStopAIMoveMulticast(this);
		}
		return;
	}
}

void ACharacterBase::MultiStopAIMove_Implementation()
{
	StopAIMove(false);
}

void ACharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACharacterBase, SingleSoundParams);
}
