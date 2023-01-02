// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"

#include "CharacterModuleTypes.h"
#include "Main/Base/ModuleNetworkComponent.h"

#include "CharacterModuleNetworkComponent.generated.h"

/**
* 
*/
UCLASS()
class WHFRAMEWORK_API UCharacterModuleNetworkComponent : public UModuleNetworkComponent
{
	GENERATED_BODY()

public:
	UCharacterModuleNetworkComponent();
	
public:
	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerPlayMontageMulticast(AActor* InCharacter, class UAnimMontage* InMontage);
	
	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerStopMontageMulticast(AActor* InCharacter, class UAnimMontage* InMontage);

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerTransformTowardsToMulticast(AActor* InCharacter, FTransform InTransform);

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerRotationTowardsMulticast(AActor* InCharacter, FRotator InRotation, float InDuration = 1.f);

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerAIMoveToMulticast(AActor* InCharacter, FVector InLocation, float InStopDistance);

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerStopAIMoveMulticast(AActor* InCharacter);
};
