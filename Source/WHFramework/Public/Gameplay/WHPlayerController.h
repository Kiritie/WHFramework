// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Math/MathTypes.h"

#include "WHPlayerController.generated.h"

class IWHPlayerInterface;
class UModuleNetworkComponent;
/**
 * 
 */
UCLASS(hidecategories = (Tick, ComponentTick, Replication, ComponentReplication, Activation, Variable, Game, Physics, Rendering, Collision, Actor, Input, Tags, LOD, Cooking, Hidden, WorldPartition, Hlod))
class WHFRAMEWORK_API AWHPlayerController : public APlayerController
{

private:
	GENERATED_BODY()

public:
	AWHPlayerController();

	//////////////////////////////////////////////////////////////////////////
	/// Defaults
public:
	UFUNCTION(BlueprintNativeEvent)
	void OnInitialize();

	UFUNCTION(BlueprintNativeEvent)
	void OnPreparatory();

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
	TMap<TSubclassOf<UModuleNetworkComponent>, UModuleNetworkComponent*> ModuleNetCompMap;
	
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

	virtual void SetupInputComponent() override;

	virtual void OnPossess(APawn* InPawn) override;

	virtual void OnUnPossess() override;

public:	
	virtual void Tick(float DeltaTime) override;
	
protected:
	virtual void Turn(float InRate);

	virtual void LookUp(float InRate);
	
	virtual void PanH(float InRate);

	virtual void PanV(float InRate);

	virtual void ZoomCam(float InRate);

	virtual void TouchPressed(ETouchIndex::Type InTouchIndex, FVector InLocation);

	virtual void TouchPressedImpl();

	virtual void TouchReleased(ETouchIndex::Type InTouchIndex, FVector InLocation);

	virtual void TouchReleasedImpl(ETouchIndex::Type InTouchIndex);

	virtual void TouchMoved(ETouchIndex::Type InTouchIndex, FVector InLocation);

	virtual void StartInteract(FKey InKey);

	virtual void EndInteract(FKey InKey);

	//////////////////////////////////////////////////////////////////////////
	/// Player
protected:
	UPROPERTY(BlueprintReadOnly)
	APawn* PlayerPawn;

public:
	template<class T>
	T* GetPlayerPawn()
	{
		return Cast<T>(PlayerPawn);
	}

	UFUNCTION(BlueprintPure)
	APawn* GetPlayerPawn() const { return PlayerPawn; }
	
	void SetPlayerPawn(APawn* InPlayerPawn) { PlayerPawn =  InPlayerPawn; }

	//////////////////////////////////////////////////////////////////////////
	/// Input
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

protected:
	virtual void MoveForward(float InValue);

	virtual void MoveRight(float InValue);

	virtual void MoveUp(float InValue);
};
