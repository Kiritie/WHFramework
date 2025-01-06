// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "CameraModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CameraModuleStatics.generated.h"

class ACameraPointBase;
class USpringArmComponent;
class UCameraComponent;
class ACameraActorBase;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UCameraModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	template<class T>
	static T* GetCurrentCamera()
	{
		return Cast<T>(GetCurrentCamera());
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "CameraModuleStatics")
	static ACameraActorBase* GetCurrentCamera(TSubclassOf<ACameraActorBase> InClass = nullptr);
	
	UFUNCTION(BlueprintPure, Category = "CameraModuleStatics")
	static ACameraPointBase* GetDefaultCameraPoint();
	
	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void SetDefaultCameraPoint(ACameraPointBase* InCameraPoint);

	UFUNCTION(BlueprintPure, Category = "CameraModuleStatics")
	static FVector GetCameraLocation(bool bReally = false, bool bRefresh = false);

	UFUNCTION(BlueprintPure, Category = "CameraModuleStatics")
	static FRotator GetCameraRotation(bool bReally = false, bool bRefresh = false);

	UFUNCTION(BlueprintPure, Category = "CameraModuleStatics")
	static float GetCameraDistance(bool bReally = false, bool bRefresh = false);

	UFUNCTION(BlueprintPure, Category = "CameraModuleStatics")
	static FVector GetCameraOffset(bool bReally = false, bool bRefresh = false);

	UFUNCTION(BlueprintPure, Category = "CameraModuleStatics")
	static float GetCameraFov(bool bReally = false, bool bRefresh = false);

	template<class T>
	static T* GetCameraByClass(TSubclassOf<ACameraActorBase> InClass = T::StaticClass())
	{
		return Cast<T>(GetCameraByClass(InClass));
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "CameraModuleStatics")
	static ACameraActorBase* GetCameraByClass(TSubclassOf<ACameraActorBase> InClass);

	template<class T>
	static T* GetCameraByName(const FName InName)
	{
		return Cast<T>(GetCameraByName(InName));
	}

	UFUNCTION(BlueprintPure, Category = "CameraModuleStatics")
	static ACameraActorBase* GetCameraByName(const FName InName);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void SwitchCamera(ACameraActorBase* InCamera, bool bReset = true, bool bInstant = false);

	template<class T>
	static void SwitchCameraByClass(bool bReset = true, bool bInstant = false, TSubclassOf<ACameraActorBase> InClass = T::StaticClass())
	{
		SwitchCameraByClass(InClass, bReset, bInstant);
	}

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void SwitchCameraByClass(TSubclassOf<ACameraActorBase> InClass, bool bReset = true, bool bInstant = false);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void SwitchCameraByName(const FName InName, bool bReset = true, bool bInstant = false);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void SwitchCameraPoint(ACameraPointBase* InCameraPoint, bool bSetAsDefault = false, bool bInstant = false);

public:
	UFUNCTION(BlueprintCallable, meta = (AdvancedDisplay = "bAllowControl,InViewEaseType,InViewDuration,bInstant"), Category = "CameraModuleStatics")
	static void StartTrackTarget(AActor* InTargetActor, ECameraTrackMode InTrackMode = ECameraTrackMode::LocationAndRotationAndDistanceOnce, ECameraViewMode InViewMode = ECameraViewMode::Smooth, ECameraViewSpace InViewSpace = ECameraViewSpace::Local, FVector InLocation = FVector(-1.f), FVector InSocketOffset = FVector(-1.f), float InYaw = -1.f, float InPitch = -1.f, float InDistance = -1.f, bool bAllowControl = true, EEaseType InViewEaseType = EEaseType::Linear, float InViewDuration = 1.f, bool bInstant = false);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void EndTrackTarget(AActor* InTargetActor = nullptr);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InCameraViewData"), Category = "CameraModuleStatics")
	static void SetCameraView(const FCameraViewData& InCameraViewData, bool bCacheData = true, bool bInstant = false);
	
	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void ResetCameraView(ECameraResetMode InCameraResetMode = ECameraResetMode::DefaultPoint, bool bInstant = false);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void SetCameraLocation(FVector InLocation, bool bInstant = false);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void DoCameraLocation(FVector InLocation, float InDuration = 1.f, EEaseType InEaseType = EEaseType::Linear);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void StopDoCameraLocation();

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void SetCameraOffset(FVector InOffset = FVector(-1.f), bool bInstant = false);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void DoCameraOffset(FVector InOffset = FVector(-1.f), float InDuration = 1.f, EEaseType InEaseType = EEaseType::Linear);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void StopDoCameraOffset();

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void SetCameraRotation(float InYaw = -1.f, float InPitch = -1.f, bool bInstant = false);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void DoCameraRotation(float InYaw = -1.f, float InPitch = -1.f, float InDuration = 1.f, EEaseType InEaseType = EEaseType::Linear);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void StopDoCameraRotation();

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void SetCameraDistance(float InDistance = -1.f, bool bInstant = false);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void DoCameraDistance(float InDistance = -1.f, float InDuration = 1.f, EEaseType InEaseType = EEaseType::Linear);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void StopDoCameraDistance();

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void SetCameraRotationAndDistance(float InYaw = -1.f, float InPitch = -1.f, float InDistance = -1.f, bool bInstant = false);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void DoCameraRotationAndDistance(float InYaw = -1.f, float InPitch = -1.f, float InDistance = -1.f, float InDuration = 1.f, EEaseType InEaseType = EEaseType::Linear);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void SetCameraTransform(FVector InLocation, float InYaw = -1.f, float InPitch = -1.f, float InDistance = -1.f, bool bInstant = false);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void DoCameraTransform(FVector InLocation, float InYaw = -1.f, float InPitch = -1.f, float InDistance = -1.f, float InDuration = 1.f, EEaseType InEaseType = EEaseType::Linear);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void StopDoCameraTransform();

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void SetCameraFov(float InFov = -1.f, bool bInstant = false);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void DoCameraFov(float InFov = -1.f, float InDuration = 1.f, EEaseType InEaseType = EEaseType::Linear, bool bForce = true);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void StopDoCameraFov();

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void AddCameraMovementInput(FVector InDirection, float InValue);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void AddCameraRotationInput(float InYaw, float InPitch);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void AddCameraDistanceInput(float InValue);
};
