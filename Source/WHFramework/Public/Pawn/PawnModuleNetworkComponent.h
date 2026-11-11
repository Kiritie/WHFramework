// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "PawnModuleTypes.h"
#include "Main/Base/ModuleNetworkComponentBase.h"
#include "TimerManager.h"

#include "PawnModuleNetworkComponent.generated.h"

class APawnBase;
class APlayerController;

/**
* 
*/
UCLASS()
class WHFRAMEWORK_API UPawnModuleNetworkComponent : public UModuleNetworkComponentBase
{
	GENERATED_BODY()

public:
	UPawnModuleNetworkComponent();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	bool RequestPawnSwitch(APawnBase* InPawn, bool bResetCamera, bool bInstant);
	virtual bool CanRequestPawnSwitch(const APlayerController* InController, const APawnBase* InPawn) const;
	virtual bool CanRequestPawnOperation(const APlayerController* InController, const AActor* InPawn) const { return false; }
	void NotifyPawnSwitchCommitted(APawnBase* InPawn, bool bResetCamera, bool bInstant);

	UFUNCTION(Server, Reliable)
	void ServerRequestPawnSwitch(FGuid WorldRoute, APawnBase* InPawn, bool bResetCamera, bool bInstant);

	UFUNCTION(Client, Reliable)
	void ClientApplyPawnView(FGuid WorldRoute, APawnBase* InPawn, bool bResetCamera, bool bInstant);
	
public:
	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerTransformTowardsToMulticast(FGuid WorldRoute, AActor* InPawn, FTransform InTransform);

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerRotationTowardsMulticast(FGuid WorldRoute, AActor* InPawn, FRotator InRotation, float InDuration = 1.f);

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerAIMoveToMulticast(FGuid WorldRoute, AActor* InPawn, FVector InLocation, float InStopDistance);

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerStopAIMoveMulticast(FGuid WorldRoute, AActor* InPawn);

private:
	void ApplyPendingPawnView();
	void QueuePawnView(APawnBase* InPawn, bool bResetCamera, bool bInstant);
	void FlushPawnView();
	TWeakObjectPtr<APawnBase> PendingViewPawn;
	TWeakObjectPtr<UWorld> PendingViewWorld;
	FTimerHandle PendingViewTimer;
	bool bPendingViewClear = false;
	bool bPendingViewInstant = false;
	TWeakObjectPtr<APawnBase> PendingServerPawn;
	TWeakObjectPtr<UWorld> PendingServerWorld;
	bool bPendingServerView = false;
	bool bPendingServerClear = false;
	bool bPendingServerReset = false;
	bool bPendingServerInstant = false;
};
