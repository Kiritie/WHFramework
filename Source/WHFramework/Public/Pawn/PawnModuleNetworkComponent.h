// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "PawnModuleTypes.h"
#include "Main/Base/ModuleNetworkComponentBase.h"

#include "PawnModuleNetworkComponent.generated.h"

/**
* 
*/
UCLASS()
class WHFRAMEWORK_API UPawnModuleNetworkComponent : public UModuleNetworkComponentBase
{
	GENERATED_BODY()

public:
	UPawnModuleNetworkComponent();
	
public:
	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerTransformTowardsToMulticast(AActor* InPawn, FTransform InTransform);

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerRotationTowardsMulticast(AActor* InPawn, FRotator InRotation, float InDuration = 1.f);

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerAIMoveToMulticast(AActor* InPawn, FVector InLocation, float InStopDistance);

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerStopAIMoveMulticast(AActor* InPawn);
};
