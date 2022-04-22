// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "WHPlayerController.generated.h"

UENUM(BlueprintType)
enum class ETrackTargetMode : uint8
{
	LocationOnly,
	LocationAndRotation,
	LocationAndRotationAndDistance,
	LocationAndRotationAndDistanceOnce
};

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
	void Initialize();

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
	T* GetModuleNetCompByClass(TSubclassOf<UModuleNetworkComponent> InModuleClass = T::StaticClass())
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

	//////////////////////////////////////////////////////////////////////////
	/// Camera Control
protected:
	UPROPERTY(EditAnywhere, Category = "CameraControl")
	bool bCameraControlAble;

	// Move
	UPROPERTY(EditAnywhere, Category = "CameraControl|Move")
	bool bCameraMoveAble;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Move")
	bool bReverseCameraMove;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Move")
	bool bClampCameraMove;

	UPROPERTY(EditAnywhere, meta = (EditCondition = "bClampCameraMove == true"), Category = "CameraControl|Move")
	FBox CameraMoveLimit;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Move")
	float CameraMoveRate;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Move")
	float CameraMoveSmooth;

	// Rotate
	UPROPERTY(EditAnywhere, Category = "CameraControl|Rotate")
	bool bCameraRotateAble;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Rotate")
	float CameraTurnRate;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Rotate")
	float CameraLookUpRate;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Rotate")
	float CameraRotateSmooth;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Rotate")
	float MinCameraPinch;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Rotate")
	float MaxCameraPinch;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Rotate")
	float InitCameraPinch;

	// Zoom
	UPROPERTY(EditAnywhere, Category = "CameraControl|Zoom")
	bool bCameraZoomAble;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Zoom")
	bool bUseNormalizedZoom;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Zoom")
	float CameraZoomRate;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Zoom")
	float CameraZoomSmooth;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Zoom")
	float MinCameraDistance;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Zoom")
	float MaxCameraDistance;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Zoom")
	float InitCameraDistance;

	//////////////////////////////////////////////////////////////////////////
	/// Camera Stats
protected:
	UPROPERTY(VisibleAnywhere, Category = "CameraStats|Location")
	FVector CurrentCameraLocation;
	
	UPROPERTY(VisibleAnywhere, Category = "CameraStats|Location")
	FVector TargetCameraLocation;

	UPROPERTY(VisibleAnywhere, Category = "CameraStats|Rotation")
	FRotator CurrentCameraRotation;
	
	UPROPERTY(VisibleAnywhere, Category = "CameraStats|Rotation")
	FRotator TargetCameraRotation;

	UPROPERTY(VisibleAnywhere, Category = "CameraStats|Distance")
	float CurrentCameraDistance;
	
	UPROPERTY(VisibleAnywhere, Category = "CameraStats|Distance")
	float TargetCameraDistance;

private:
	UPROPERTY()
	AActor* TrackTargetActor;
	FVector TrackLocationOffset;
	float TrackYawOffset;
	float TrackPitchOffset;
	float TrackDistance;
	bool TrackAllowControl;
	ETrackTargetMode TrackTargetMode;

	int32 TouchPressedCount;
	FVector2D TouchLocationPrevious;
	float TouchPinchValuePrevious;

	FTimerHandle TouchReleaseTimerHandle1;
	FTimerHandle TouchReleaseTimerHandle2;
	FTimerHandle TouchReleaseTimerHandle3;

	UPROPERTY()
	class USpringArmComponent* CameraBoom;
	
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

	virtual void TrackTarget(bool bInstant = false);

public:
	UFUNCTION(BlueprintCallable)
	virtual void StartTrackTarget(AActor* InTargetActor, ETrackTargetMode InTrackTargetMode = ETrackTargetMode::LocationAndRotationAndDistanceOnce, FVector InLocationOffset = FVector(-1.f), float InYawOffset = -1.f, float InPitchOffset = -1.f, float InDistance = -1.f, bool bAllowControl = true, bool bInstant = false);

	UFUNCTION(BlueprintCallable)
	virtual void EndTrackTarget();

	UFUNCTION(BlueprintCallable)
	virtual void SetCameraLocation(FVector InLocation, bool bInstant = false);

	UFUNCTION(BlueprintCallable)
	virtual void SetCameraRotation(float InYaw = -1.f, float InPitch = -1.f, bool bInstant = false);

	UFUNCTION(BlueprintCallable)
	virtual void SetCameraDistance(float InDistance = -1.f, bool bInstant = false);

	UFUNCTION(BlueprintCallable)
	virtual void SetCameraRotationAndDistance(float InYaw = -1.f, float InPitch = -1.f, float InDistance = -1.f, bool bInstant = false);

	UFUNCTION(BlueprintCallable)
	virtual void AddCameraMovementInput(FVector InDirection, float InValue);

	UFUNCTION(BlueprintCallable)
	virtual void AddCameraRotationInput(float InYaw, float InPitch);

	UFUNCTION(BlueprintCallable)
	virtual void AddCameraDistanceInput(float InValue);

public:
	UFUNCTION(BlueprintPure)
	bool IsControllingMove() const;

	UFUNCTION(BlueprintPure)
	bool IsControllingRotate() const;

	UFUNCTION(BlueprintPure)
	bool IsControllingZoom() const;
	
	UFUNCTION(BlueprintPure)
	class USpringArmComponent* GetCameraBoom();

public:
	UFUNCTION(BlueprintPure)
	bool IsCameraControlAble() const { return bCameraControlAble; }

	UFUNCTION(BlueprintCallable)
	void SetCameraControlAble(bool bInCameraControlAble) { this->bCameraControlAble = bInCameraControlAble; }

	UFUNCTION(BlueprintPure)
	bool IsCameraMoveAble() const { return bCameraMoveAble; }

	UFUNCTION(BlueprintCallable)
	void SetCameraMoveAble(bool bInCameraMoveAble) { this->bCameraMoveAble = bInCameraMoveAble; }

	UFUNCTION(BlueprintPure)
	bool IsCameraRotateAble() const { return bCameraRotateAble; }

	UFUNCTION(BlueprintCallable)
	void SetCameraRotateAble(bool bInCameraRotateAble) { this->bCameraRotateAble = bInCameraRotateAble; }

	UFUNCTION(BlueprintPure)
	bool IsCameraZoomAble() const { return bCameraZoomAble; }

	UFUNCTION(BlueprintCallable)
	void SetCameraZoomAble(bool bInCameraZoomAble) { this->bCameraZoomAble = bInCameraZoomAble; }

	UFUNCTION(BlueprintCallable)
	void SetCameraControlStates(bool bInCameraControlAble, bool bInCameraMoveAble, bool bInCameraRotateAble, bool bInCameraZoomAble);

	UFUNCTION(BlueprintPure)
	float GetCameraMoveRate() const { return CameraMoveRate; }

	UFUNCTION(BlueprintCallable)
	void SetCameraMoveRate(float InCameraMoveRate) { this->CameraMoveRate = InCameraMoveRate; }

	UFUNCTION(BlueprintPure)
	float GetCameraMoveSmooth() const { return CameraMoveSmooth; }

	UFUNCTION(BlueprintCallable)
	void SetCameraMoveSmooth(float InCameraMoveSmooth) { this->CameraMoveSmooth = InCameraMoveSmooth; }

	UFUNCTION(BlueprintPure)
	float GetCameraTurnRate() const { return CameraTurnRate; }

	UFUNCTION(BlueprintCallable)
	void SetCameraTurnRate(float InCameraTurnRate) { this->CameraTurnRate = InCameraTurnRate; }

	UFUNCTION(BlueprintPure)
	float GetCameraLookUpRate() const { return CameraLookUpRate; }

	UFUNCTION(BlueprintCallable)
	void SetCameraLookUpRate(float InCameraLookUpRate) { this->CameraLookUpRate = InCameraLookUpRate; }

	UFUNCTION(BlueprintPure)
	float GetCameraRotateSmooth() const { return CameraRotateSmooth; }

	UFUNCTION(BlueprintCallable)
	void SetCameraRotateSmooth(float InCameraRotateSmooth) { this->CameraRotateSmooth = InCameraRotateSmooth; }

	UFUNCTION(BlueprintPure)
	float GetMinCameraPinch() const { return MinCameraPinch; }

	UFUNCTION(BlueprintCallable)
	void SetMinCameraPinch(float InMinCameraPinch) { this->MinCameraPinch = InMinCameraPinch; }

	UFUNCTION(BlueprintPure)
	float GetMaxCameraPinch() const { return MaxCameraPinch; }

	UFUNCTION(BlueprintCallable)
	void SetMaxCameraPinch(float InMaxCameraPinch) { this->MaxCameraPinch = InMaxCameraPinch; }

	UFUNCTION(BlueprintPure)
	float GetInitCameraPinch() const { return InitCameraPinch; }

	UFUNCTION(BlueprintCallable)
	void SetInitCameraPinch(float InInitCameraPinch) { this->InitCameraPinch = InInitCameraPinch; }

	UFUNCTION(BlueprintPure)
	float GetCameraZoomRate() const { return CameraZoomRate; }

	UFUNCTION(BlueprintCallable)
	void SetCameraZoomRate(float InCameraZoomRate) { this->CameraZoomRate = InCameraZoomRate; }

	UFUNCTION(BlueprintPure)
	float GetCameraZoomSmooth() const { return CameraZoomSmooth; }

	UFUNCTION(BlueprintCallable)
	void SetCameraZoomSmooth(float InCameraZoomSmooth) { this->CameraZoomSmooth = InCameraZoomSmooth; }

	UFUNCTION(BlueprintPure)
	float GetMinCameraDistance() const { return MinCameraDistance; }

	UFUNCTION(BlueprintCallable)
	void SetMinCameraDistance(float InMinCameraDistance) { this->MinCameraDistance = InMinCameraDistance; }

	UFUNCTION(BlueprintPure)
	float GetMaxCameraDistance() const { return MaxCameraDistance; }

	UFUNCTION(BlueprintCallable)
	void SetMaxCameraDistance(float InMaxCameraDistance) { this->MaxCameraDistance = InMaxCameraDistance; }

	UFUNCTION(BlueprintPure)
	float GetInitCameraDistance() const { return InitCameraDistance; }

	UFUNCTION(BlueprintCallable)
	void SetInitCameraDistance(float InInitCameraDistance) { this->InitCameraDistance = InInitCameraDistance; }
	
	UFUNCTION(BlueprintPure)
	FVector GetCurrentCameraLocation() const { return CurrentCameraLocation; }

	UFUNCTION(BlueprintPure)
	FRotator GetCurrentCameraRotation() const { return CurrentCameraRotation; }

	UFUNCTION(BlueprintPure)
	float GetCurrentCameraDistance() const { return CurrentCameraDistance; }
	
	UFUNCTION(BlueprintPure)
	FVector GetTargetCameraLocation() const { return TargetCameraLocation; }

	UFUNCTION(BlueprintPure)
	FRotator GetTargetCameraRotation() const { return TargetCameraRotation; }

	UFUNCTION(BlueprintPure)
	float GetTargetCameraDistance() const { return TargetCameraDistance; }

	//////////////////////////////////////////////////////////////////////////
	/// PlayerController
protected:	
	UPROPERTY(EditAnywhere, Category = "PlayerController")
	bool bUse2DInputMode;

	UPROPERTY(EditAnywhere, Category = "PlayerController")
	float TouchInputRate;

protected:
	virtual void MoveForward(float InValue);

	virtual void MoveRight(float InValue);

	virtual void MoveUp(float InValue);
};
