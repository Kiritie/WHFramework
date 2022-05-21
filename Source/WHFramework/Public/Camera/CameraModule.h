// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "CameraModuleTypes.h"
#include "Main/Base/ModuleBase.h"
#include "Math/MathTypes.h"

#include "CameraModule.generated.h"

class USpringArmComponent;
class AWHPlayerController;
class ACameraPawnBase;
UCLASS()
class WHFRAMEWORK_API ACameraModule : public AModuleBase
{
	GENERATED_BODY()

public:
	// ParamSets default values for this actor's properties
	ACameraModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate_Implementation() override;

	virtual void OnDestroy_Implementation() override;
#endif

	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation() override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnPause_Implementation() override;

	virtual void OnUnPause_Implementation() override;

	//////////////////////////////////////////////////////////////////////////
	/// CameraModule
protected:
	UPROPERTY(EditAnywhere, Category = "CameraModule")
	TSubclassOf<ACameraPawnBase> DefaultCameraClass;

	UPROPERTY(EditAnywhere, meta = (EditCondition = "DefaultCameraClass != nullptr"), Category = "CameraModule")
	bool DefaultInstantSwitch;

	UPROPERTY(EditAnywhere, Category = "CameraModule")
	TArray<TSubclassOf<ACameraPawnBase>> CameraClasses;
	
	UPROPERTY(EditAnywhere, Category = "CameraModule")
	TArray<ACameraPawnBase*> Cameras;
		
	UPROPERTY(VisibleAnywhere, Category = "CameraModule")
	ACameraPawnBase* CurrentCamera;

private:			
	UPROPERTY(Transient)
	TMap<FName, ACameraPawnBase*> CameraMap;

public:
	template<class T>
	T* GetCurrentCamera(TSubclassOf<ACameraPawnBase> InClass = T::StaticClass())
	{
		return Cast<T>(K2_GetCurrentCamera(InClass));
	}

	UFUNCTION(BlueprintPure)
	ACameraPawnBase* K2_GetCurrentCamera(TSubclassOf<ACameraPawnBase> InClass);

	UFUNCTION(BlueprintPure)
	USpringArmComponent* GetCurrentCameraBoom();

	template<class T>
	T* GetCamera(TSubclassOf<ACameraPawnBase> InClass = T::StaticClass())
	{
		return Cast<T>(K2_GetCamera(InClass));
	}

	UFUNCTION(BlueprintCallable, DisplayName = "GetCamera", meta = (DeterminesOutputType = "InClass"))
	ACameraPawnBase* K2_GetCamera(TSubclassOf<ACameraPawnBase> InClass);

	template<class T>
	T* GetCameraByName(const FName InCameraName)
	{
		return Cast<T>(K2_GetCameraByName(InCameraName));
	}

	UFUNCTION(BlueprintPure, DisplayName = "GetCamera")
	ACameraPawnBase* K2_GetCameraByName(const FName InCameraName) const;

	template<class T>
	void SwitchCamera(bool bInstant = false, TSubclassOf<ACameraPawnBase> InClass = T::StaticClass())
	{
		K2_SwitchCamera(InClass, bInstant);
	}

	UFUNCTION(BlueprintCallable, DisplayName = "SwitchCamera")
	void K2_SwitchCamera(TSubclassOf<ACameraPawnBase> InClass, bool bInstant = false);

	UFUNCTION(BlueprintCallable)
	void SwitchCameraByName(const FName InCameraName, bool bInstant = false);

	//////////////////////////////////////////////////////////////////////////
	/// Camera Control
protected:
	UPROPERTY(EditAnywhere, Category = "CameraControl")
	bool bCameraControlAble;

	// Move
	UPROPERTY(EditAnywhere, Category = "CameraControl|Move")
	bool bCameraMoveAble;

	UPROPERTY(EditAnywhere, meta = (EditCondition = "bCameraMoveAble == true"), Category = "CameraControl|Move")
	bool bCameraMoveControlAble;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Move")
	bool bReverseCameraPanMove;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Move")
	bool bClampCameraMove;

	UPROPERTY(EditAnywhere, meta = (EditCondition = "bClampCameraMove == true"), Category = "CameraControl|Move")
	FBox CameraMoveRange;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Move")
	FKey CameraPanMoveKey;
	
	UPROPERTY(EditAnywhere, Category = "CameraControl|Move")
	float CameraMoveRate;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Move")
	float CameraMoveSpeed;

	// Rotate
	UPROPERTY(EditAnywhere, Category = "CameraControl|Rotate")
	bool bCameraRotateAble;

	UPROPERTY(EditAnywhere, meta = (EditCondition = "bCameraRotateAble == true"), Category = "CameraControl|Rotate")
	bool bCameraRotateControlAble;

	UPROPERTY(EditAnywhere, meta = (EditCondition = "bCameraRotateAble == true"), Category = "CameraControl|Rotate")
	bool bReverseCameraPitch;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Rotate")
	FKey CameraRotateKey;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Rotate")
	float CameraTurnRate;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Rotate")
	float CameraLookUpRate;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Rotate")
	float CameraRotateSpeed;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Rotate")
	float MinCameraPitch;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Rotate")
	float MaxCameraPitch;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Rotate")
	float InitCameraPitch;

	// Zoom
	UPROPERTY(EditAnywhere, Category = "CameraControl|Zoom")
	bool bCameraZoomAble;

	UPROPERTY(EditAnywhere, meta = (EditCondition = "bCameraZoomAble == true"), Category = "CameraControl|Zoom")
	bool bCameraZoomControlAble;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Zoom")
	bool bUseNormalizedZoom;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Zoom")
	FKey CameraZoomKey;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Zoom")
	float CameraZoomRate;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Zoom")
	float CameraZoomSpeed;

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

	UPROPERTY(Transient)
	AWHPlayerController* PlayerController;
	
private:
	float CameraDoMoveTime;
	float CameraDoMoveDuration;
	FVector CameraDoMoveLocation;
	EEaseType CameraDoMoveEaseType;
	float CameraDoRotateTime;
	float CameraDoRotateDuration;
	FRotator CameraDoRotateRotation;
	EEaseType CameraDoRotateEaseType;
	float CameraDoZoomTime;
	float CameraDoZoomDuration;
	float CameraDoZoomDistance;
	EEaseType CameraDoZoomEaseType;
	UPROPERTY()
	AActor* TrackTargetActor;
	FVector TrackLocationOffset;
	float TrackYawOffset;
	float TrackPitchOffset;
	float TrackDistance;
	bool TrackAllowControl;
	ETrackTargetMode TrackTargetMode;

protected:
	virtual void DoTrackTarget(bool bInstant = false);

public:
	UFUNCTION(BlueprintCallable)
	virtual void StartTrackTarget(AActor* InTargetActor, ETrackTargetMode InTrackTargetMode = ETrackTargetMode::LocationAndRotationAndDistanceOnce, ETrackTargetSpace InTrackTargetSpace = ETrackTargetSpace::Local, FVector InLocationOffset = FVector(-1.f), float InYawOffset = -1.f, float InPitchOffset = -1.f, float InDistance = -1.f, bool bAllowControl = true, bool bInstant = false);

	UFUNCTION(BlueprintCallable)
	virtual void EndTrackTarget(AActor* InTargetActor = nullptr);

	UFUNCTION(BlueprintCallable)
	virtual void SetCameraLocation(FVector InLocation, bool bInstant = false);

	UFUNCTION(BlueprintCallable)
	virtual void DoCameraLocation(FVector InLocation, float InDuration = 1.f, EEaseType InEaseType = EEaseType::Linear);

	UFUNCTION(BlueprintCallable)
	virtual void StopDoCameraLocation();

	UFUNCTION(BlueprintCallable)
	virtual void SetCameraRotation(float InYaw = -1.f, float InPitch = -1.f, bool bInstant = false);

	UFUNCTION(BlueprintCallable)
	virtual void DoCameraRotation(float InYaw = -1.f, float InPitch = -1.f, float InDuration = 1.f, EEaseType InEaseType = EEaseType::Linear);

	UFUNCTION(BlueprintCallable)
	virtual void StopDoCameraRotation();

	UFUNCTION(BlueprintCallable)
	virtual void SetCameraDistance(float InDistance = -1.f, bool bInstant = false);

	UFUNCTION(BlueprintCallable)
	virtual void DoCameraDistance(float InDistance = -1.f, float InDuration = 1.f, EEaseType InEaseType = EEaseType::Linear);

	UFUNCTION(BlueprintCallable)
	virtual void StopDoCameraDistance();

	UFUNCTION(BlueprintCallable)
	virtual void SetCameraRotationAndDistance(float InYaw = -1.f, float InPitch = -1.f, float InDistance = -1.f, bool bInstant = false);

	UFUNCTION(BlueprintCallable)
	virtual void DoCameraRotationAndDistance(float InYaw = -1.f, float InPitch = -1.f, float InDistance = -1.f, float InDuration = 1.f, EEaseType InEaseType = EEaseType::Linear);

	UFUNCTION(BlueprintCallable)
	virtual void SetCameraTransform(FVector InLocation, float InYaw = -1.f, float InPitch = -1.f, float InDistance = -1.f, bool bInstant = false);

	UFUNCTION(BlueprintCallable)
	virtual void DoCameraTransform(FVector InLocation, float InYaw = -1.f, float InPitch = -1.f, float InDistance = -1.f, float InDuration = 1.f, EEaseType InEaseType = EEaseType::Linear);

	UFUNCTION(BlueprintCallable)
	virtual void StopDoCameraTransform();

	UFUNCTION(BlueprintCallable)
	virtual void AddCameraMovementInput(FVector InDirection, float InValue);

	UFUNCTION(BlueprintCallable)
	virtual void AddCameraRotationInput(float InYaw, float InPitch);

	UFUNCTION(BlueprintCallable)
	virtual void AddCameraDistanceInput(float InValue);

public:
	UFUNCTION(BlueprintPure)
	bool IsControllingMove();

	UFUNCTION(BlueprintPure)
	bool IsControllingRotate();

	UFUNCTION(BlueprintPure)
	bool IsControllingZoom();

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
	bool IsCameraMoveControlAble() const { return bCameraMoveControlAble; }

	UFUNCTION(BlueprintCallable)
	void SetCameraMoveControlAble(bool bInCameraMoveControlAble) { this->bCameraMoveControlAble = bInCameraMoveControlAble; }

	UFUNCTION(BlueprintPure)
	FKey GetCameraPanMoveKey() const { return CameraPanMoveKey; }

	UFUNCTION(BlueprintPure)
	bool IsReverseCameraPanMove() const { return bReverseCameraPanMove; }

	UFUNCTION(BlueprintPure)
	float GetCameraMoveRate() const { return CameraMoveRate; }

	UFUNCTION(BlueprintCallable)
	void SetCameraMoveRate(float InCameraMoveRate) { this->CameraMoveRate = InCameraMoveRate; }

	UFUNCTION(BlueprintPure)
	float GetCameraMoveSpeed() const { return CameraMoveSpeed; }

	UFUNCTION(BlueprintCallable)
	void SetCameraMoveSpeed(float InCameraMoveSpeed) { this->CameraMoveSpeed = InCameraMoveSpeed; }

	UFUNCTION(BlueprintPure)
	bool IsCameraRotateAble() const { return bCameraRotateAble; }

	UFUNCTION(BlueprintCallable)
	void SetCameraRotateAble(bool bInCameraRotateAble) { this->bCameraRotateAble = bInCameraRotateAble; }

	UFUNCTION(BlueprintPure)
	bool IsCameraRotateControlAble() const { return bCameraRotateControlAble; }

	UFUNCTION(BlueprintCallable)
	void SetCameraRotateControlAble(bool bInCameraRotateControlAble) { this->bCameraRotateControlAble = bInCameraRotateControlAble; }

	UFUNCTION(BlueprintPure)
	bool IsReverseCameraPitch() const { return bReverseCameraPitch; }

	UFUNCTION(BlueprintPure)
	FKey GetCameraRotateKey() const { return CameraRotateKey; }

	UFUNCTION(BlueprintPure)
	float GetCameraTurnRate() const { return CameraTurnRate; }

	UFUNCTION(BlueprintCallable)
	void SetCameraTurnRate(float InCameraTurnRate) { this->CameraTurnRate = InCameraTurnRate; }

	UFUNCTION(BlueprintPure)
	float GetCameraLookUpRate() const { return CameraLookUpRate; }

	UFUNCTION(BlueprintCallable)
	void SetCameraLookUpRate(float InCameraLookUpRate) { this->CameraLookUpRate = InCameraLookUpRate; }

	UFUNCTION(BlueprintPure)
	float GetCameraRotateSpeed() const { return CameraRotateSpeed; }

	UFUNCTION(BlueprintCallable)
	void SetCameraRotateSpeed(float InCameraRotateSpeed) { this->CameraRotateSpeed = InCameraRotateSpeed; }

	UFUNCTION(BlueprintPure)
	float GetMinCameraPitch() const { return MinCameraPitch; }

	UFUNCTION(BlueprintCallable)
	void SetMinCameraPitch(float InMinCameraPitch) { this->MinCameraPitch = InMinCameraPitch; }

	UFUNCTION(BlueprintPure)
	float GetMaxCameraPitch() const { return MaxCameraPitch; }

	UFUNCTION(BlueprintCallable)
	void SetMaxCameraPitch(float InMaxCameraPitch) { this->MaxCameraPitch = InMaxCameraPitch; }

	UFUNCTION(BlueprintPure)
	float GetInitCameraPitch() const { return InitCameraPitch; }

	UFUNCTION(BlueprintCallable)
	void SetInitCameraPitch(float InInitCameraPitch) { this->InitCameraPitch = InInitCameraPitch; }

	UFUNCTION(BlueprintPure)
	bool IsCameraZoomAble() const { return bCameraZoomAble; }

	UFUNCTION(BlueprintCallable)
	void SetCameraZoomAble(bool bInCameraZoomAble) { this->bCameraZoomAble = bInCameraZoomAble; }

	UFUNCTION(BlueprintPure)
	bool IsCameraZoomControlAble() const { return bCameraZoomControlAble; }

	UFUNCTION(BlueprintCallable)
	void SetCameraZoomControlAble(bool bInCameraZoomControlAble) { this->bCameraZoomControlAble = bInCameraZoomControlAble; }

	UFUNCTION(BlueprintPure)
	FKey GetCameraZoomKey() const { return CameraZoomKey; }

	UFUNCTION(BlueprintPure)
	float GetCameraZoomRate() const { return CameraZoomRate; }

	UFUNCTION(BlueprintCallable)
	void SetCameraZoomRate(float InCameraZoomRate) { this->CameraZoomRate = InCameraZoomRate; }

	UFUNCTION(BlueprintPure)
	float GetCameraZoomSpeed() const { return CameraZoomSpeed; }

	UFUNCTION(BlueprintCallable)
	void SetCameraZoomSpeed(float InCameraZoomSpeed) { this->CameraZoomSpeed = InCameraZoomSpeed; }

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
	
	UFUNCTION(BlueprintPure)
	FVector GetRealCameraLocation();

	UFUNCTION(BlueprintPure)
	FRotator GetRealCameraRotation();

protected:
	AWHPlayerController* GetPlayerController();

	//////////////////////////////////////////////////////////////////////////
	/// Network
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
