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
		Character->PlayMontage(InMontage, true);
	}
}

bool UCharacterModuleNetworkComponent::ServerStopMontageMulticast_Validate(AActor* InCharacter, UAnimMontage* InMontage) { return true; }
void UCharacterModuleNetworkComponent::ServerStopMontageMulticast_Implementation(AActor* InCharacter, UAnimMontage* InMontage)
{
	if(ICharacterInterface* Character = Cast<ICharacterInterface>(InCharacter))
	{
		Character->StopMontage(InMontage, true);
	}
}

bool UCharacterModuleNetworkComponent::ServerTransformTowardsToMulticast_Validate(AActor* InCharacter, FTransform InTransform) { return true; }
void UCharacterModuleNetworkComponent::ServerTransformTowardsToMulticast_Implementation(AActor* InCharacter, FTransform InTransform)
{
	if(ICharacterInterface* Character = Cast<ICharacterInterface>(InCharacter))
	{
		Character->TransformTowards(InTransform, true);
	}
}

bool UCharacterModuleNetworkComponent::ServerRotationTowardsMulticast_Validate(AActor* InCharacter, FRotator InRotation, float InDuration) { return true; }
void UCharacterModuleNetworkComponent::ServerRotationTowardsMulticast_Implementation(AActor* InCharacter, FRotator InRotation, float InDuration)
{
	if(ICharacterInterface* Character = Cast<ICharacterInterface>(InCharacter))
	{
		Character->RotationTowards(InRotation, InDuration, true);
	}
}

bool UCharacterModuleNetworkComponent::ServerAIMoveToMulticast_Validate(AActor* InCharacter, FVector InLocation, float InStopDistance) { return true; }
void UCharacterModuleNetworkComponent::ServerAIMoveToMulticast_Implementation(AActor* InCharacter, FVector InLocation, float InStopDistance)
{
	if(ICharacterInterface* Character = Cast<ICharacterInterface>(InCharacter))
	{
		Character->AIMoveTo(InLocation, InStopDistance, true);
	}
}

bool UCharacterModuleNetworkComponent::ServerStopAIMoveMulticast_Validate(AActor* InCharacter) { return true; }
void UCharacterModuleNetworkComponent::ServerStopAIMoveMulticast_Implementation(AActor* InCharacter)
{
	if(ICharacterInterface* Character = Cast<ICharacterInterface>(InCharacter))
	{
		Character->StopAIMove(true);
	}
}
