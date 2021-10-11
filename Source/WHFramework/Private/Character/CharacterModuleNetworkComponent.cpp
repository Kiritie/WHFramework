// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/CharacterModuleNetworkComponent.h"

#include "Character/Base/CharacterBase.h"
#include "Character/Base/CharacterInterface.h"

UCharacterModuleNetworkComponent::UCharacterModuleNetworkComponent()
{
	
}

bool UCharacterModuleNetworkComponent::ServerPlayMontageMulticast_Validate(AActor* InCharacter, UAnimMontage* InMontage) { return true; }
void UCharacterModuleNetworkComponent::ServerPlayMontageMulticast_Implementation(AActor* InCharacter, UAnimMontage* InMontage)
{
	if(ICharacterInterface* Character = Cast<ICharacterInterface>(InCharacter))
	{
		Character->Execute_PlayMontage(InCharacter, InMontage, true);
	}
}

bool UCharacterModuleNetworkComponent::ServerStopMontageMulticast_Validate(AActor* InCharacter, UAnimMontage* InMontage) { return true; }
void UCharacterModuleNetworkComponent::ServerStopMontageMulticast_Implementation(AActor* InCharacter, UAnimMontage* InMontage)
{
	if(ICharacterInterface* Character = Cast<ICharacterInterface>(InCharacter))
	{
		Character->Execute_StopMontage(InCharacter, InMontage, true);
	}
}

bool UCharacterModuleNetworkComponent::ServerTeleportToMulticast_Validate(AActor* InCharacter, FTransform InTransform) { return true; }
void UCharacterModuleNetworkComponent::ServerTeleportToMulticast_Implementation(AActor* InCharacter, FTransform InTransform)
{
	if(ICharacterInterface* Character = Cast<ICharacterInterface>(InCharacter))
	{
		Character->Execute_TeleportTo(InCharacter, InTransform, true);
	}
}

bool UCharacterModuleNetworkComponent::ServerAIMoveToMulticast_Validate(AActor* InCharacter, FTransform InTransform) { return true; }
void UCharacterModuleNetworkComponent::ServerAIMoveToMulticast_Implementation(AActor* InCharacter, FTransform InTransform)
{
	if(ICharacterInterface* Character = Cast<ICharacterInterface>(InCharacter))
	{
		Character->Execute_AIMoveTo(InCharacter, InTransform, true);
	}
}

bool UCharacterModuleNetworkComponent::ServerStopAIMoveMulticast_Validate(AActor* InCharacter) { return true; }
void UCharacterModuleNetworkComponent::ServerStopAIMoveMulticast_Implementation(AActor* InCharacter)
{
	if(ICharacterInterface* Character = Cast<ICharacterInterface>(InCharacter))
	{
		Character->Execute_StopAIMove(InCharacter, true);
	}
}

bool UCharacterModuleNetworkComponent::ServerRotationTowardsMulticast_Validate(AActor* InCharacter, FRotator InRotation, float InDuration) { return true; }
void UCharacterModuleNetworkComponent::ServerRotationTowardsMulticast_Implementation(AActor* InCharacter, FRotator InRotation, float InDuration)
{
	if(ICharacterInterface* Character = Cast<ICharacterInterface>(InCharacter))
	{
		Character->Execute_RotationTowards(InCharacter, InRotation, InDuration, true);
	}
}
