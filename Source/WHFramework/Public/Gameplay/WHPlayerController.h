// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Common/Base/WHActor.h"
#include "GameFramework/PlayerController.h"

#include "WHPlayerController.generated.h"

class IWHPlayerInterface;
class UModuleNetworkComponentBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerPawnChanged, class APawn*, InPlayerPawn);

/**
 * 
 */
UCLASS(hidecategories = (Tick, ComponentTick, Replication, ComponentReplication, Activation, Variable, Game, Physics, Rendering, Collision, Actor, Input, Tags, LOD, Cooking, Hidden, WorldPartition, Hlod))
class WHFRAMEWORK_API AWHPlayerController : public APlayerController, public IWHActorInterface
{

private:
	GENERATED_BODY()

public:
	AWHPlayerController();

	//////////////////////////////////////////////////////////////////////////
	/// WHActor
public:
	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnTermination_Implementation(EPhase InPhase) override;

protected:
	virtual bool IsDefaultLifecycle_Implementation() const override { return true; }

	//////////////////////////////////////////////////////////////////////////
	/// Components
protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	class UWidgetInteractionComponent* WidgetInteractionComp;

	//////////////////////////////////////////////////////////////////////////
	/// Inherits
protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	virtual void OnPossess(APawn* InPawn) override;

	virtual void OnUnPossess() override;

	virtual void SetupInputComponent() override;

public:
	virtual void Tick(float DeltaSeconds) override;
	
	//////////////////////////////////////////////////////////////////////////
	/// Raycast
public:
	virtual bool RaycastSingleFromAimPoint(float InRayDistance, ECollisionChannel InGameTraceChannel, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult) const;

	virtual bool RaycastSingleFromMousePosition(float InRayDistance, ECollisionChannel InGameTraceChannel, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult) const;

	//////////////////////////////////////////////////////////////////////////
	/// Player
protected:
	UPROPERTY(BlueprintReadOnly, Category = "Player")
	APawn* PlayerPawn;

public:
	UPROPERTY(BlueprintAssignable, Category = "Player")
	FOnPlayerPawnChanged OnPlayerPawnChanged;

protected:
	virtual void SetPlayerPawn(APawn* InPlayerPawn);

public:
	template<class T>
	T* GetPlayerPawn()
	{
		return Cast<T>(PlayerPawn);
	}

	UFUNCTION(BlueprintPure)
	APawn* GetPlayerPawn() const { return PlayerPawn; }
};
