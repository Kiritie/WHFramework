// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Global/Base/WHActor.h"
#include "GameFramework/PlayerController.h"
#include "Math/MathTypes.h"

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

	virtual void OnTermination_Implementation() override;

	//////////////////////////////////////////////////////////////////////////
	/// Components
protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	class UWidgetInteractionComponent* WidgetInteractionComp;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	class UAudioModuleNetworkComponent* AudioModuleNetComp;
	
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	class UCameraModuleNetworkComponent* CameraModuleNetComp;
	
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	class UCharacterModuleNetworkComponent* CharacterModuleNetComp;
	
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	class UEventModuleNetworkComponent* EventModuleNetComp;
	
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	class UMediaModuleNetworkComponent* MediaModuleNetComp;
		
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	class UNetworkModuleNetworkComponent* NetworkModuleNetComp;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	class UProcedureModuleNetworkComponent* ProcedureModuleNetComp;

private:
	UPROPERTY(Transient)
	TMap<TSubclassOf<UModuleNetworkComponentBase>, UModuleNetworkComponentBase*> ModuleNetCompMap;
	
public:
	template<class T>
	T* GetModuleNetCompByClass(TSubclassOf<T> InModuleClass = T::StaticClass())
	{
		if(ModuleNetCompMap.Contains(InModuleClass))
		{
			return Cast<T>(ModuleNetCompMap[InModuleClass]);
		}
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	/// Inherits
protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	virtual void SetupInputComponent() override;

	virtual void OnPossess(APawn* InPawn) override;

	virtual void OnUnPossess() override;

public:
	virtual void Tick(float DeltaTime) override;
	
	//////////////////////////////////////////////////////////////////////////
	/// Raycast
public:
	virtual bool RaycastSingleFromAimPoint(float InRayDistance, ECollisionChannel InGameTraceType, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult) const;

	//////////////////////////////////////////////////////////////////////////
	/// Input
protected:
	virtual void TurnCam(float InRate);

	virtual void LookUpCam(float InRate);
	
	virtual void PanHCam(float InRate);

	virtual void PanVCam(float InRate);

	virtual void ZoomCam(float InRate);

	virtual void TouchPressed(ETouchIndex::Type InTouchIndex, FVector InLocation);

	virtual void TouchPressedImpl();

	virtual void TouchReleased(ETouchIndex::Type InTouchIndex, FVector InLocation);

	virtual void TouchReleasedImpl(ETouchIndex::Type InTouchIndex);

	virtual void TouchMoved(ETouchIndex::Type InTouchIndex, FVector InLocation);

	virtual void StartInteract(FKey InKey);

	virtual void EndInteract(FKey InKey);

protected:
	UPROPERTY(EditAnywhere, Category = "Input")
	float TouchInputRate;

	UPROPERTY(VisibleAnywhere, Category = "Input")
	int32 TouchPressedCount;

public:
	UFUNCTION(BlueprintPure)
	int32 GetTouchPressedCount() const { return TouchPressedCount; }

private:
	FVector2D TouchLocationPrevious;
	float TouchPinchValuePrevious;
	FTimerHandle TouchReleaseTimerHandle1;
	FTimerHandle TouchReleaseTimerHandle2;
	FTimerHandle TouchReleaseTimerHandle3;

	//////////////////////////////////////////////////////////////////////////
	/// Player
public:
	virtual void TurnPlayer(float InValue);
	
	virtual void MoveHPlayer(float InValue);

	virtual void MoveVPlayer(float InValue);

	virtual void MoveForwardPlayer(float InValue);

	virtual void MoveRightPlayer(float InValue);

	virtual void MoveUpPlayer(float InValue);

protected:
	UPROPERTY(BlueprintReadOnly)
	APawn* PlayerPawn;

	UPROPERTY(BlueprintAssignable)
	FOnPlayerPawnChanged OnPlayerPawnChanged;

public:
	template<class T>
	T* GetPlayerPawn()
	{
		return Cast<T>(PlayerPawn);
	}

	UFUNCTION(BlueprintPure)
	APawn* GetPlayerPawn() const { return PlayerPawn; }

	FOnPlayerPawnChanged& GetOnPlayerPawnChanged() { return OnPlayerPawnChanged; }

protected:
	virtual void SetPlayerPawn(APawn* InPlayerPawn);
};
