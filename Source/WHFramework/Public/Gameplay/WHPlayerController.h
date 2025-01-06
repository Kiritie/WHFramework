// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Common/Base/WHActor.h"
#include "GameFramework/PlayerController.h"

#include "WHPlayerController.generated.h"

class IInteractionAgentInterface;
class IWHPlayerInterface;
class UModuleNetworkComponentBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerPawnChanged, class APawn*, InPlayerPawn);

UENUM(BlueprintType)
enum class EInteractionRaycastMode : uint8
{
	None,
	FromAimPosition,
	FromMousePosition
};

/**
 * 
 */
UCLASS(hidecategories = (Tick, ComponentTick, Replication, ComponentReplication, Activation, Variable, Game, Physics, Rendering, Collision, Actor, Input, Tags, LOD, Cooking, Hidden, Hlod), meta=(ShortTooltip="A Player Controller is an actor responsible for controlling a Pawn used by the player."))
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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WHActor")
	bool bInitialized;
	
protected:
	virtual bool IsInitialized_Implementation() const override { return bInitialized; }
	
	virtual bool IsUseDefaultLifecycle_Implementation() const override { return true; }

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

	virtual void InitPlayerState() override;

public:
	virtual void Tick(float DeltaSeconds) override;
	
	//////////////////////////////////////////////////////////////////////////
	/// Interaction
protected:
	UPROPERTY(EditAnywhere, Category = "Interaction")
	EInteractionRaycastMode InteractionRaycastMode;

	UPROPERTY(EditAnywhere, Category = "Interaction")
	float InteractionDistance;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Interaction")
	TScriptInterface<IInteractionAgentInterface> HoveringInteraction;

	UPROPERTY(VisibleAnywhere, Category = "Interaction")
	TScriptInterface<IInteractionAgentInterface> SelectedInteraction;

protected:
	UFUNCTION(BlueprintNativeEvent)
	void RefreshInteraction();

public:
	UFUNCTION(BlueprintPure)
	EInteractionRaycastMode GetInteractionRaycastMode() const { return InteractionRaycastMode; }

	UFUNCTION(BlueprintCallable)
	void SetInteractionRaycastMode(EInteractionRaycastMode InInteractionRaycastMode) { InteractionRaycastMode = InInteractionRaycastMode; }
	
	UFUNCTION(BlueprintPure)
	virtual TScriptInterface<IInteractionAgentInterface> GetHoveringInteraction() { return HoveringInteraction; }
	
	UFUNCTION(BlueprintPure)
	virtual TScriptInterface<IInteractionAgentInterface> GetSelectedInteraction() { return SelectedInteraction; }
	
	UFUNCTION(BlueprintCallable)
	virtual void SetSelectedInteraction(TScriptInterface<IInteractionAgentInterface> InInteractionAgent);

	//////////////////////////////////////////////////////////////////////////
	/// Raycast
public:
	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InIgnoreActors"))
	virtual bool RaycastSingleFromScreenPosition(FVector2D InScreenPosition, float InRayDistance, ECollisionChannel InGameTraceChannel, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InIgnoreActors"))
	virtual bool RaycastSingleFromViewportPosition(FVector2D InViewportPosition, float InRayDistance, ECollisionChannel InGameTraceChannel, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InIgnoreActors"))
	virtual bool RaycastSingleFromMousePosition(float InRayDistance, ECollisionChannel InGameTraceChannel, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InIgnoreActors"))
	virtual bool RaycastSingleFromAimPosition(float InRayDistance, ECollisionChannel InGameTraceChannel, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult);

public:
	UFUNCTION(BlueprintPure)
	virtual FVector2D GetAnimPosition() const { return FVector2D(0.5f); }

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
	
	UFUNCTION()
	virtual void OnPlayerDestroyed(AActor* InPlayerActor);

public:
	template<class T>
	T* GetPlayerPawn()
	{
		return Cast<T>(PlayerPawn);
	}

	UFUNCTION(BlueprintPure)
	APawn* GetPlayerPawn() const { return PlayerPawn; }
};
