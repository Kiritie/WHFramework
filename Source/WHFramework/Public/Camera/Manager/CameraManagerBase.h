// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Camera/CameraModuleTypes.h"
#include "Camera/PlayerCameraManager.h"
#include "Math/MathTypes.h"

#include "CameraManagerBase.generated.h"

class ACameraPointBase;
class ACameraActorBase;
/**
 *
 */
UCLASS(notplaceable, MinimalAPI)
class ACameraManagerBase : public APlayerCameraManager
{
	GENERATED_BODY()

	friend class UCameraModule;

public:
	ACameraManagerBase(const FObjectInitializer& ObjectInitializer);

public:
	virtual void InitializeFor(APlayerController* PC) override;

	virtual void Destroyed() override;

	virtual void SetViewTarget(AActor* NewViewTarget, FViewTargetTransitionParams TransitionParams) override;

	virtual void UpdateCamera(float DeltaTime) override;

protected:
	virtual void UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TArray<ACameraActorBase*> Cameras;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	ACameraActorBase* CurrentCamera;
		
	UPROPERTY(EditAnywhere, Category = "Camera")
	ACameraPointBase* DefaultCameraPoint;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	ACameraPointBase* CurrentCameraPoint;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	int32 LocalPlayerIndex;

private:
	TMap<FName, ACameraActorBase*> CameraMap;

public:
	template<class T>
	T* GetCurrentCamera()
	{
		return Cast<T>(CurrentCamera);
	}
	
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	ACameraActorBase* GetCurrentCamera(TSubclassOf<ACameraActorBase> InClass = nullptr) const;
	
	UFUNCTION(BlueprintPure)
	ACameraPointBase* GetDefaultCameraPoint() const { return DefaultCameraPoint; }
	
	UFUNCTION(BlueprintCallable)
	void SetDefaultCameraPoint(ACameraPointBase* InCameraPoint) { DefaultCameraPoint = InCameraPoint; }

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

	UFUNCTION(BlueprintPure)
	int32 GetLocalPlayerIndex() const { return LocalPlayerIndex; }
	
	UFUNCTION(BlueprintCallable)
	void SwitchCamera(ACameraActorBase* InCamera, bool bReset = true, bool bInstant = false);

	template<class T>
	void SwitchCameraByClass(bool bReset = true, bool bInstant = false, TSubclassOf<ACameraActorBase> InClass = T::StaticClass())
	{
		SwitchCameraByClass(InClass, bReset, bInstant);
	}

	UFUNCTION(BlueprintCallable)
	void SwitchCameraByClass(TSubclassOf<ACameraActorBase> InClass, bool bReset = true, bool bInstant = false);

	UFUNCTION(BlueprintCallable)
	void SwitchCameraByName(const FName InName, bool bReset = true, bool bInstant = false);

	UFUNCTION(BlueprintCallable)
	void SwitchCameraPoint(ACameraPointBase* InCameraPoint, bool bSetAsDefault = false, bool bInstant = false);

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

	UPROPERTY(EditAnywhere, Category = "CameraStats|Rotate")
	float InitCameraPitch;

	UPROPERTY(VisibleAnywhere, Category = "CameraStats|Distance")
	float CurrentCameraDistance;
	
	UPROPERTY(VisibleAnywhere, Category = "CameraStats|Distance")
	float TargetCameraDistance;

	UPROPERTY(EditAnywhere, Category = "CameraStats|Zoom")
	float InitCameraDistance;

	UPROPERTY(VisibleAnywhere, Category = "CameraStats|Offset")
	FVector CurrentCameraOffset;
	
	UPROPERTY(VisibleAnywhere, Category = "CameraStats|Offset")
	FVector TargetCameraOffset;

	UPROPERTY(EditAnywhere, Category = "CameraStats|Offset")
	FVector InitCameraOffset;

	UPROPERTY(VisibleAnywhere, Category = "CameraStats|Fov")
	float CurrentCameraFov;
	
	UPROPERTY(VisibleAnywhere, Category = "CameraStats|Fov")
	float TargetCameraFov;
	
	UPROPERTY(EditAnywhere, Category = "CameraStats|Fov")
	float InitCameraFov;

private:
	float CameraDoLocationTime;
	float CameraDoLocationDuration;
	FVector CameraDoLocationLocation;
	EEaseType CameraDoLocationEaseType;
	
	float CameraDoOffsetTime;
	float CameraDoOffsetDuration;
	FVector CameraDoOffsetOffset;
	EEaseType CameraDoOffsetEaseType;
	
	float CameraDoRotationTime;
	float CameraDoRotationDuration;
	FRotator CameraDoRotationRotation;
	EEaseType CameraDoRotationEaseType;
	
	float CameraDoDistanceTime;
	float CameraDoDistanceDuration;
	float CameraDoDistanceDistance;
	EEaseType CameraDoDistanceEaseType;
		
	float CameraDoFovTime;
	float CameraDoFovDuration;
	float CameraDoFovFov;
	EEaseType CameraDoFovEaseType;

	FCameraViewData CachedCameraViewData;
	FCameraViewData TrackCameraViewData;
	bool bTrackAllowControl;
	ECameraSmoothMode TrackSmoothMode;
	ECameraControlMode TrackControlMode;

protected:
	virtual void DoTrackTarget(bool bInstant = false);
	
	virtual void DoTrackTargetLocation(bool bInstant = false);
	
	virtual void DoTrackTargetRotation(bool bInstant = false);
	
	virtual void DoTrackTargetDistance(bool bInstant = false);

public:
	UFUNCTION(BlueprintCallable, meta = (AdvancedDisplay = "bAllowControl,InViewEaseType,InViewDuration,bInstant"))
	virtual void StartTrackTarget(AActor* InTargetActor, ECameraTrackMode InTrackMode = ECameraTrackMode::LocationAndRotationAndDistanceOnce, ECameraViewMode InViewMode = ECameraViewMode::Smooth, ECameraViewSpace InViewSpace = ECameraViewSpace::Local, FVector InLocation = FVector(-1.f), FVector InOffset = FVector(-1.f), float InYaw = -1.f, float InPitch = -1.f, float InDistance = -1.f, bool bAllowControl = true, EEaseType InViewEaseType = EEaseType::Linear, float InViewDuration = 1.f, bool bInstant = false);

	UFUNCTION(BlueprintCallable)
	virtual void EndTrackTarget(AActor* InTargetActor = nullptr);

	UFUNCTION(BlueprintCallable)
	virtual void SetCameraLocation(FVector InLocation, bool bInstant = false);

	UFUNCTION(BlueprintCallable)
	virtual void DoCameraLocation(FVector InLocation, float InDuration = 1.f, EEaseType InEaseType = EEaseType::Linear, bool bForce = true);

	UFUNCTION(BlueprintCallable)
	virtual void StopDoCameraLocation();

	UFUNCTION(BlueprintCallable)
	virtual void SetCameraOffset(FVector InOffset = FVector(-1.f), bool bInstant = false);

	UFUNCTION(BlueprintCallable)
	virtual void DoCameraOffset(FVector InOffset = FVector(-1.f), float InDuration = 1.f, EEaseType InEaseType = EEaseType::Linear, bool bForce = true);

	UFUNCTION(BlueprintCallable)
	virtual void StopDoCameraOffset();

	UFUNCTION(BlueprintCallable)
	virtual void SetCameraRotation(float InYaw = -1.f, float InPitch = -1.f, bool bInstant = false);

	UFUNCTION(BlueprintCallable)
	virtual void DoCameraRotation(float InYaw = -1.f, float InPitch = -1.f, float InDuration = 1.f, EEaseType InEaseType = EEaseType::Linear, bool bForce = true);

	UFUNCTION(BlueprintCallable)
	virtual void StopDoCameraRotation();

	UFUNCTION(BlueprintCallable)
	virtual void SetCameraDistance(float InDistance = -1.f, bool bInstant = false);

	UFUNCTION(BlueprintCallable)
	virtual void DoCameraDistance(float InDistance = -1.f, float InDuration = 1.f, EEaseType InEaseType = EEaseType::Linear, bool bForce = true);

	UFUNCTION(BlueprintCallable)
	virtual void StopDoCameraDistance();

	UFUNCTION(BlueprintCallable)
	virtual void SetCameraRotationAndDistance(float InYaw = -1.f, float InPitch = -1.f, float InDistance = -1.f, bool bInstant = false);

	UFUNCTION(BlueprintCallable)
	virtual void DoCameraRotationAndDistance(float InYaw = -1.f, float InPitch = -1.f, float InDistance = -1.f, float InDuration = 1.f, EEaseType InEaseType = EEaseType::Linear, bool bForce = true);

	UFUNCTION(BlueprintCallable)
	virtual void SetCameraTransform(FVector InLocation, float InYaw = -1.f, float InPitch = -1.f, float InDistance = -1.f, bool bInstant = false);

	UFUNCTION(BlueprintCallable)
	virtual void DoCameraTransform(FVector InLocation, float InYaw = -1.f, float InPitch = -1.f, float InDistance = -1.f, float InDuration = 1.f, EEaseType InEaseType = EEaseType::Linear, bool bForce = true);

	UFUNCTION(BlueprintCallable)
	virtual void StopDoCameraTransform();

	UFUNCTION(BlueprintCallable)
	virtual void SetCameraFov(float InFov = -1.f, bool bInstant = false);

	UFUNCTION(BlueprintCallable)
	virtual void DoCameraFov(float InFov = -1.f, float InDuration = 1.f, EEaseType InEaseType = EEaseType::Linear, bool bForce = true);

	UFUNCTION(BlueprintCallable)
	virtual void StopDoCameraFov();

	UFUNCTION(BlueprintCallable)
	virtual void AddCameraMovementInput(FVector InDirection, float InValue);

	UFUNCTION(BlueprintCallable)
	virtual void AddCameraRotationInput(float InYaw, float InPitch);

	UFUNCTION(BlueprintCallable)
	virtual void AddCameraDistanceInput(float InValue);

public:
	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InCameraViewData"))
	virtual void SetCameraView(const FCameraViewData& InCameraViewData, bool bCacheData = true, bool bInstant = false);
	
	UFUNCTION(BlueprintCallable)
	virtual void ResetCameraView(ECameraResetMode InCameraResetMode = ECameraResetMode::DefaultPoint, bool bInstant = false);

protected:
	UFUNCTION(BlueprintCallable)
	virtual void SetCameraViewParams(const FCameraViewParams& InCameraViewParams, bool bInstant = false);

public:
	UFUNCTION(BlueprintPure)
	bool IsControllingMove();

	UFUNCTION(BlueprintPure)
	bool IsControllingRotate();

	UFUNCTION(BlueprintPure)
	bool IsControllingZoom();

	UFUNCTION(BlueprintPure)
	bool IsTrackingTarget() const;

public:
	UFUNCTION(BlueprintPure)
	float GetMinCameraPitch() const;

	UFUNCTION(BlueprintPure)
	float GetMaxCameraPitch() const;

	UFUNCTION(BlueprintPure)
	float GetInitCameraPitch() const { return InitCameraPitch; }

	UFUNCTION(BlueprintCallable)
	void SetInitCameraPitch(float InInitCameraPitch) { InitCameraPitch = InInitCameraPitch; }

	UFUNCTION(BlueprintPure)
	float GetInitCameraDistance() const { return InitCameraDistance; }

	UFUNCTION(BlueprintCallable)
	void SetInitCameraDistance(float InInitCameraDistance) { InitCameraDistance = InInitCameraDistance; }
	
	UFUNCTION(BlueprintPure)
	FVector GetRealCameraLocation();
	
	UFUNCTION(BlueprintPure)
	FVector GetCurrentCameraLocation(bool bRefresh = false) const;

	UFUNCTION(BlueprintPure)
	FVector GetTargetCameraLocation() const { return TargetCameraLocation; }

	UFUNCTION(BlueprintPure)
	FVector GetRealCameraOffset() const;

	UFUNCTION(BlueprintPure)
	FVector GetCurrentCameraOffset(bool bRefresh = false) const;

	UFUNCTION(BlueprintPure)
	FVector GetTargetCameraOffset() const { return TargetCameraOffset; }

	UFUNCTION(BlueprintPure)
	FRotator GetRealCameraRotation();

	UFUNCTION(BlueprintPure)
	FRotator GetCurrentCameraRotation(bool bRefresh = false);

	UFUNCTION(BlueprintPure)
	FRotator GetTargetCameraRotation() const { return TargetCameraRotation; }

	UFUNCTION(BlueprintPure)
	float GetRealCameraDistance() const;

	UFUNCTION(BlueprintPure)
	float GetCurrentCameraDistance(bool bRefresh = false) const;

	UFUNCTION(BlueprintPure)
	float GetTargetCameraDistance() const { return TargetCameraDistance; }

	UFUNCTION(BlueprintPure)
	float GetRealCameraFov() const;

	UFUNCTION(BlueprintPure)
	float GetCurrentCameraFov(bool bRefresh = false) const;

	UFUNCTION(BlueprintPure)
	float GetTargetCameraFov() const { return TargetCameraFov; }

	UFUNCTION(BlueprintPure)
	AActor* GetTrackingTarget() const { return TrackCameraViewData.CameraViewParams.CameraViewTarget; }

	//////////////////////////////////////////////////////////////////////////
	/// Network
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
