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
UCLASS(notplaceable)
class WHFRAMEWORK_API ACameraManagerBase : public APlayerCameraManager
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

	UPROPERTY(Transient)
	FCameraSettings RuntimeSettings;

	UPROPERTY(Transient)
	bool bOwnsRuntimeCameras;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	FCameraRuntimeState RuntimeState;

	UPROPERTY(Transient)
	FCameraTrackProfile TrackProfile;

	TWeakObjectPtr<AActor> TrackTarget;

private:
	TMap<FName, ACameraActorBase*> CameraMap;

	void RefreshTransitionState();

public:
	void ApplySettings(const FCameraSettings& InSettings) { RuntimeSettings = InSettings; }

	UFUNCTION(BlueprintPure)
	const FCameraRuntimeState& GetRuntimeState() const { return RuntimeState; }

	UFUNCTION(BlueprintCallable)
	void BindTarget(const FCameraTargetRequest& InRequest);

	UFUNCTION(BlueprintCallable)
	void ClearTarget(AActor* InExpectedTarget = nullptr);

	UFUNCTION(BlueprintCallable)
	void ApplyView(const FCameraViewRequest& InRequest);

	UFUNCTION(BlueprintCallable)
	void StopViewTransition(int32 InProperties);

	UFUNCTION(BlueprintCallable)
	void AddLookInput(const FVector2D& InValue);

	UFUNCTION(BlueprintCallable)
	void AddPanInput(const FVector2D& InValue);

	UFUNCTION(BlueprintCallable)
	void AddMoveInput(const FVector& InValue);

	UFUNCTION(BlueprintCallable)
	void AddZoomInput(float InValue);

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

protected:
	virtual void ApplyLocationInternal(FVector InLocation, bool bInstant);

	virtual void TransitionLocationInternal(FVector InLocation, float InDuration, EEaseType InEaseType, bool bForce = true);

	virtual void StopTransitionLocationInternal();

	virtual void ApplyOffsetInternal(FVector InOffset, bool bInstant = false);

	virtual void TransitionOffsetInternal(FVector InOffset, float InDuration, EEaseType InEaseType, bool bForce = true);

	virtual void StopTransitionOffsetInternal();

	virtual void ApplyRotationInternal(float InYaw, float InPitch, bool bInstant);

	virtual void TransitionRotationInternal(float InYaw, float InPitch, float InDuration, EEaseType InEaseType, bool bForce = true);

	virtual void StopTransitionRotationInternal();

	virtual void ApplyDistanceInternal(float InDistance, bool bInstant);

	virtual void TransitionDistanceInternal(float InDistance, float InDuration, EEaseType InEaseType, bool bForce = true);

	virtual void StopTransitionDistanceInternal();

	virtual void ApplyRotationAndDistanceInternal(float InYaw, float InPitch, float InDistance, bool bInstant);

	virtual void TransitionRotationAndDistanceInternal(float InYaw, float InPitch, float InDistance, float InDuration, EEaseType InEaseType, bool bForce = true);

	virtual void ApplyTransformInternal(FVector InLocation, float InYaw, float InPitch, float InDistance, bool bInstant);

	virtual void TransitionTransformInternal(FVector InLocation, float InYaw, float InPitch, float InDistance, float InDuration, EEaseType InEaseType, bool bForce = true);

	virtual void StopTransitionTransformInternal();

	virtual void ApplyFovInternal(float InFov, bool bInstant);

	virtual void TransitionFovInternal(float InFov, float InDuration, EEaseType InEaseType, bool bForce = true);

	virtual void StopTransitionFovInternal();

	virtual void ApplyMoveInputInternal(FVector InDirection, float InValue);

	virtual void ApplyLookInputInternal(float InYaw, float InPitch);

	virtual void ApplyZoomInputInternal(float InValue);

public:
	virtual void ApplyViewData(const FCameraViewData& InCameraViewData, bool bCacheData = true, bool bInstant = false);
	
	UFUNCTION(BlueprintCallable)
	virtual void ResetView(ECameraResetMode InCameraResetMode = ECameraResetMode::DefaultPoint, bool bInstant = false);

protected:
	virtual void ApplyViewParamsInternal(const FCameraViewParams& InCameraViewParams, bool bInstant);

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
	AActor* GetTrackingTarget() const { return RuntimeState.Target; }

	//////////////////////////////////////////////////////////////////////////
	/// Network
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
