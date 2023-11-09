// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "CameraModuleTypes.h"
#include "Main/Base/ModuleBase.h"
#include "Math/MathTypes.h"
#include "SaveGame/Base/SaveDataInterface.h"

#include "CameraModule.generated.h"

class UCameraComponent;
class USpringArmComponent;
class AWHPlayerController;
class ACameraActorBase;
class ACameraManagerBase;

UCLASS()
class WHFRAMEWORK_API UCameraModule : public UModuleBase
{
	GENERATED_BODY()
	
	GENERATED_MODULE(UCameraModule)

public:
	// ParamSets default values for this actor's properties
	UCameraModule();

	~UCameraModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
#endif

	virtual void OnInitialize() override;

	virtual void OnPreparatory(EPhase InPhase) override;

	virtual void OnRefresh(float DeltaSeconds) override;

	virtual void OnPause() override;

	virtual void OnUnPause() override;

	virtual void OnTermination(EPhase InPhase) override;

protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual void UnloadData(EPhase InPhase) override;

	virtual FSaveData* ToData() override;

	//////////////////////////////////////////////////////////////////////////
	/// Camera
protected:
	UPROPERTY(EditAnywhere, Category = "Camera")
	TArray<TSubclassOf<ACameraActorBase>> CameraClasses;
	
	UPROPERTY(EditAnywhere, Category = "Camera")
	TArray<ACameraActorBase*> Cameras;
	
	UPROPERTY(EditAnywhere, Category = "Camera")
	ACameraActorBase* DefaultCamera;

	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "DefaultCamera != nullptr"), Category = "Camera")
	bool DefaultInstantSwitch;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	ACameraActorBase* CurrentCamera;

private:
	UPROPERTY(Transient)
	TMap<FName, ACameraActorBase*> CameraMap;

public:
	template<class T>
	T* GetCurrentCamera()
	{
		return Cast<T>(CurrentCamera);
	}
	
	UFUNCTION(BlueprintPure)
	ACameraActorBase* GetCurrentCamera(TSubclassOf<ACameraActorBase> InClass = nullptr) const;

	UFUNCTION(BlueprintPure)
	ACameraManagerBase* GetCurrentCameraManager();

	template<class T>
	T* GetCameraByClass(TSubclassOf<ACameraActorBase> InClass = T::StaticClass())
	{
		return Cast<T>(GetCameraByClass(InClass));
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass"))
	ACameraActorBase* GetCameraByClass(TSubclassOf<ACameraActorBase> InClass);

	template<class T>
	T* GetCameraByName(const FName InName)
	{
		return Cast<T>(GetCameraByName(InName));
	}

	UFUNCTION(BlueprintPure)
	ACameraActorBase* GetCameraByName(const FName InName) const;

	UFUNCTION(BlueprintCallable)
	void SwitchCamera(ACameraActorBase* InCamera, bool bInstant = false);

	template<class T>
	void SwitchCameraByClass(bool bInstant = false, TSubclassOf<ACameraActorBase> InClass = T::StaticClass())
	{
		SwitchCameraByClass(InClass, bInstant);
	}

	UFUNCTION(BlueprintCallable)
	void SwitchCameraByClass(TSubclassOf<ACameraActorBase> InClass, bool bInstant = false);

	UFUNCTION(BlueprintCallable)
	void SwitchCameraByName(const FName InName, bool bInstant = false);

	//////////////////////////////////////////////////////////////////////////
	/// Camera Control
protected:
	UPROPERTY(EditAnywhere, Category = "CameraControl")
	bool bCameraControlAble;

	// Move
	UPROPERTY(EditAnywhere, Category = "CameraControl|Move")
	bool bCameraMoveAble;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Move")
	bool bCameraMoveControlAble;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Move")
	bool bReverseCameraPanMove;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Move")
	bool bClampCameraMove;

	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "bClampCameraMove == true"), Category = "CameraControl|Move")
	FBox CameraMoveRange;
	
	UPROPERTY(EditAnywhere, Category = "CameraControl|Move")
	float CameraMoveRate;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Move")
	bool bSmoothCameraMove;

	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "bSmoothCameraMove == true"), Category = "CameraControl|Move")
	float CameraMoveSpeed;

	// Rotate
	UPROPERTY(EditAnywhere, Category = "CameraControl|Rotate")
	bool bCameraRotateAble;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Rotate")
	bool bCameraRotateControlAble;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Rotate")
	bool bReverseCameraPitch;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Rotate")
	float CameraTurnRate;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Rotate")
	float CameraLookUpRate;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Rotate")
	bool bSmoothCameraRotate;

	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "bSmoothCameraRotate == true"), Category = "CameraControl|Rotate")
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

	UPROPERTY(EditAnywhere, Category = "CameraControl|Zoom")
	bool bCameraZoomControlAble;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Zoom")
	bool bNormalizeCameraZoom;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Zoom")
	float CameraZoomRate;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Zoom")
	bool bSmoothCameraZoom;

	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "bSmoothCameraZoom == true"), Category = "CameraControl|Zoom")
	float CameraZoomSpeed;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Zoom")
	float InitCameraDistance;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Zoom")
	float MinCameraDistance;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Zoom")
	float MaxCameraDistance;

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
	FVector InitSocketOffset;
	UPROPERTY()
	AActor* TrackTargetActor;
	FVector TrackLocationOffset;
	float TrackYawOffset;
	float TrackPitchOffset;
	float TrackDistance;
	bool bTrackAllowControl;
	bool bIsControllingMove;
	bool bIsControllingRotate;
	bool bIsControllingZoom;
	ETrackTargetMode TrackTargetMode;
	UPROPERTY(Transient)
	AWHPlayerController* PlayerController;

protected:
	virtual void DoTrackTarget(bool bInstant = false);

public:
	UFUNCTION(BlueprintCallable)
	virtual void StartTrackTarget(AActor* InTargetActor, ETrackTargetMode InTrackTargetMode = ETrackTargetMode::LocationAndRotationAndDistanceOnce, ETrackTargetSpace InTrackTargetSpace = ETrackTargetSpace::Local, FVector InLocationOffset = FVector(-1.f), FVector InSocketOffset = FVector(-1.f), float InYawOffset = -1.f, float InPitchOffset = -1.f, float InDistance = -1.f, bool bAllowControl = true, bool bInstant = false);

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
	void SetCameraControlAble(bool bInCameraControlAble) { bCameraControlAble = bInCameraControlAble; }

	UFUNCTION(BlueprintCallable)
	void SetCameraMovementAble(bool bInCameraMoveAble, bool bInCameraRotateAble, bool bInCameraZoomAble) { bCameraMoveAble = bInCameraMoveAble; bCameraRotateAble = bInCameraRotateAble; bCameraZoomAble = bInCameraZoomAble; }

	UFUNCTION(BlueprintCallable)
	void SetCameraMovementControlAble(bool bInCameraMoveControlAble, bool bInCameraRotateControlAble, bool bInCameraZoomControlAble) { bCameraMoveControlAble = bInCameraMoveControlAble; bCameraRotateControlAble = bInCameraRotateControlAble; bCameraZoomControlAble = bInCameraZoomControlAble; }

	UFUNCTION(BlueprintPure)
	bool IsCameraMoveAble() const { return bCameraMoveAble; }

	UFUNCTION(BlueprintCallable)
	void SetCameraMoveAble(bool bInCameraMoveAble) { bCameraMoveAble = bInCameraMoveAble; }

	UFUNCTION(BlueprintPure)
	bool IsCameraMoveControlAble() const { return bCameraMoveControlAble; }

	UFUNCTION(BlueprintCallable)
	void SetCameraMoveControlAble(bool bInCameraMoveControlAble) { bCameraMoveControlAble = bInCameraMoveControlAble; }

	UFUNCTION(BlueprintPure)
	bool IsReverseCameraPanMove() const { return bReverseCameraPanMove; }

	UFUNCTION(BlueprintCallable)
	void SetReverseCameraPanMove(bool bInReverseCameraPanMove) { bReverseCameraPanMove = bInReverseCameraPanMove; }

	UFUNCTION(BlueprintPure)
	float GetCameraMoveRate() const { return CameraMoveRate; }

	UFUNCTION(BlueprintCallable)
	void SetCameraMoveRate(float InCameraMoveRate) { CameraMoveRate = InCameraMoveRate; }

	UFUNCTION(BlueprintPure)
	bool IsSmoothCameraMove() const { return bSmoothCameraMove; }

	UFUNCTION(BlueprintCallable)
	void SetSmoothCameraMove(bool bInSmoothCameraMove) { bSmoothCameraMove = bInSmoothCameraMove; }

	UFUNCTION(BlueprintPure)
	float GetCameraMoveSpeed() const { return CameraMoveSpeed; }

	UFUNCTION(BlueprintCallable)
	void SetCameraMoveSpeed(float InCameraMoveSpeed) { CameraMoveSpeed = InCameraMoveSpeed; }

	UFUNCTION(BlueprintPure)
	bool IsCameraRotateAble() const { return bCameraRotateAble; }

	UFUNCTION(BlueprintCallable)
	void SetCameraRotateAble(bool bInCameraRotateAble) { bCameraRotateAble = bInCameraRotateAble; }

	UFUNCTION(BlueprintPure)
	bool IsCameraRotateControlAble() const { return bCameraRotateControlAble; }

	UFUNCTION(BlueprintCallable)
	void SetCameraRotateControlAble(bool bInCameraRotateControlAble) { bCameraRotateControlAble = bInCameraRotateControlAble; }

	UFUNCTION(BlueprintPure)
	bool IsReverseCameraPitch() const { return bReverseCameraPitch; }

	UFUNCTION(BlueprintCallable)
	void SetReverseCameraPitch(bool bInReverseCameraPitch) { bReverseCameraPitch = bInReverseCameraPitch; }

	UFUNCTION(BlueprintPure)
	float GetCameraTurnRate() const { return CameraTurnRate; }

	UFUNCTION(BlueprintCallable)
	void SetCameraTurnRate(float InCameraTurnRate) { CameraTurnRate = InCameraTurnRate; }

	UFUNCTION(BlueprintPure)
	float GetCameraLookUpRate() const { return CameraLookUpRate; }

	UFUNCTION(BlueprintCallable)
	void SetCameraLookUpRate(float InCameraLookUpRate) { CameraLookUpRate = InCameraLookUpRate; }

	UFUNCTION(BlueprintPure)
	bool IsSmoothCameraRotate() const { return bSmoothCameraRotate; }

	UFUNCTION(BlueprintCallable)
	void SetSmoothCameraRotate(bool bInSmoothCameraRotate) { bSmoothCameraRotate = bInSmoothCameraRotate; }

	UFUNCTION(BlueprintPure)
	float GetCameraRotateSpeed() const { return CameraRotateSpeed; }

	UFUNCTION(BlueprintCallable)
	void SetCameraRotateSpeed(float InCameraRotateSpeed) { CameraRotateSpeed = InCameraRotateSpeed; }

	UFUNCTION(BlueprintPure)
	float GetMinCameraPitch() const { return MinCameraPitch; }

	UFUNCTION(BlueprintCallable)
	void SetMinCameraPitch(float InMinCameraPitch) { MinCameraPitch = InMinCameraPitch; }

	UFUNCTION(BlueprintPure)
	float GetMaxCameraPitch() const { return MaxCameraPitch; }

	UFUNCTION(BlueprintCallable)
	void SetMaxCameraPitch(float InMaxCameraPitch) { MaxCameraPitch = InMaxCameraPitch; }

	UFUNCTION(BlueprintPure)
	float GetInitCameraPitch() const { return InitCameraPitch; }

	UFUNCTION(BlueprintCallable)
	void SetInitCameraPitch(float InInitCameraPitch) { InitCameraPitch = InInitCameraPitch; }

	UFUNCTION(BlueprintPure)
	bool IsCameraZoomAble() const { return bCameraZoomAble; }

	UFUNCTION(BlueprintCallable)
	void SetCameraZoomAble(bool bInCameraZoomAble) { bCameraZoomAble = bInCameraZoomAble; }

	UFUNCTION(BlueprintPure)
	bool IsCameraZoomControlAble() const { return bCameraZoomControlAble; }

	UFUNCTION(BlueprintCallable)
	void SetCameraZoomControlAble(bool bInCameraZoomControlAble) { bCameraZoomControlAble = bInCameraZoomControlAble; }

	UFUNCTION(BlueprintPure)
	float GetCameraZoomRate() const { return CameraZoomRate; }

	UFUNCTION(BlueprintCallable)
	void SetCameraZoomRate(float InCameraZoomRate) { CameraZoomRate = InCameraZoomRate; }

	UFUNCTION(BlueprintPure)
	bool IsSmoothCameraZoom() const { return bSmoothCameraZoom; }

	UFUNCTION(BlueprintCallable)
	void SetSmoothCameraZoom(bool bInSmoothCameraZoom) { bSmoothCameraZoom = bInSmoothCameraZoom; }

	UFUNCTION(BlueprintPure)
	float GetCameraZoomSpeed() const { return CameraZoomSpeed; }

	UFUNCTION(BlueprintCallable)
	void SetCameraZoomSpeed(float InCameraZoomSpeed) { CameraZoomSpeed = InCameraZoomSpeed; }

	UFUNCTION(BlueprintPure)
	float GetMinCameraDistance() const { return MinCameraDistance; }

	UFUNCTION(BlueprintCallable)
	void SetMinCameraDistance(float InMinCameraDistance) { MinCameraDistance = InMinCameraDistance; }

	UFUNCTION(BlueprintPure)
	float GetMaxCameraDistance() const { return MaxCameraDistance; }

	UFUNCTION(BlueprintCallable)
	void SetMaxCameraDistance(float InMaxCameraDistance) { MaxCameraDistance = InMaxCameraDistance; }

	UFUNCTION(BlueprintPure)
	float GetInitCameraDistance() const { return InitCameraDistance; }

	UFUNCTION(BlueprintCallable)
	void SetInitCameraDistance(float InInitCameraDistance) { InitCameraDistance = InInitCameraDistance; }
	
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

protected:
	AWHPlayerController* GetPlayerController();

	//////////////////////////////////////////////////////////////////////////
	/// Network
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
