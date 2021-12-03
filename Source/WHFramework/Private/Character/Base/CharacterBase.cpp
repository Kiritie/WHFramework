// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Base/CharacterBase.h"

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

ACharacterBase::ACharacterBase()
{
	bReplicates = true;

	CharacterName = NAME_None;
	CharacterAnim = nullptr;

	ScenePoint = CreateDefaultSubobject<USceneComponent>(FName("ScenePoint"));
	ScenePoint->SetupAttachment(RootComponent);

	SingleSound = CreateDefaultSubobject<UAudioComponent>(FName("SingleSound"));
	SingleSound->SetupAttachment(RootComponent);
}

void ACharacterBase::BeginPlay()
{
	Super::BeginPlay();

	USceneModuleBPLibrary::SetSceneObject(CharacterName, this);

	CharacterAnim = Cast<UCharacterAnim>(GetMesh()->GetAnimInstance());
}

void ACharacterBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

USceneComponent* ACharacterBase::GetScenePoint_Implementation() const
{
	return ScenePoint;
}

FName ACharacterBase::GetCharacterName_Implementation() const
{
	return CharacterName;
}

UCharacterAnim* ACharacterBase::GetCharacterAnim_Implementation() const
{
	return CharacterAnim;
}

void ACharacterBase::StartSpeak_Implementation()
{
	
}

void ACharacterBase::StopSpeak_Implementation()
{
	
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

void ACharacterBase::PlaySound_Implementation(USoundBase* InSound, float InVolume, bool bMulticast)
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

	Execute_StartSpeak(this);
}

void ACharacterBase::StopSound_Implementation()
{
	SingleSoundParams.Sound = nullptr;
	OnRep_SingleSoundParams();
	GetWorld()->GetTimerManager().ClearTimer(SingleSoundStopTimerHandle);

	Execute_StopSpeak(this);
}

void ACharacterBase::PlayMontage_Implementation(UAnimMontage* InMontage, bool bMulticast)
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
	if(UAnimInstance* AnimInstance = Execute_GetCharacterAnim(this))
	{
		AnimInstance->Montage_Play(InMontage);
	}
}

void ACharacterBase::MultiPlayMontage_Implementation(UAnimMontage* InMontage)
{
	Execute_PlayMontage(this, InMontage, false);
}

void ACharacterBase::PlayMontageByName_Implementation(const FName InMontageName, bool bMulticast)
{
	
}

void ACharacterBase::StopMontage_Implementation(UAnimMontage* InMontage, bool bMulticast)
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
	if(UAnimInstance* AnimInstance = Execute_GetCharacterAnim(this))
	{
		AnimInstance->Montage_Stop(.2f, InMontage);
	}
}

void ACharacterBase::MultiStopMontage_Implementation(UAnimMontage* InMontage)
{
	Execute_StopMontage(this, InMontage, false);
}

void ACharacterBase::StopMontageByName_Implementation(const FName InMontageName, bool bMulticast)
{
	
}

void ACharacterBase::TeleportTo_Implementation(FTransform InTransform, bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
		{
			MultiTeleportTo(InTransform);
		}
		else if(UCharacterModuleNetworkComponent* CharacterModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UCharacterModuleNetworkComponent>())
		{
			CharacterModuleNetworkComponent->ServerTeleportToMulticast(this, InTransform);
		}
		return;
	}
	SetActorTransform(InTransform);
}

void ACharacterBase::MultiTeleportTo_Implementation(FTransform InTransform)
{
	Execute_TeleportTo(this, InTransform, false);
}

void ACharacterBase::AIMoveTo_Implementation(FTransform InTransform, bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
		{
			MultiAIMoveTo(InTransform);
		}
		else if(UCharacterModuleNetworkComponent* CharacterModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UCharacterModuleNetworkComponent>())
		{
			CharacterModuleNetworkComponent->ServerAIMoveToMulticast(this, InTransform);
		}
		return;
	}
}

void ACharacterBase::MultiAIMoveTo_Implementation(FTransform InTransform)
{
	Execute_AIMoveTo(this, InTransform, false);
}

void ACharacterBase::StopAIMove_Implementation(bool bMulticast)
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
	Execute_StopAIMove(this, false);
}

void ACharacterBase::RotationTowards_Implementation(FRotator InRotation, float InDuration, bool bMulticast)
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
	Execute_RotationTowards(this, InRotation, InDuration, false);
}

void ACharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACharacterBase, SingleSoundParams);
}
