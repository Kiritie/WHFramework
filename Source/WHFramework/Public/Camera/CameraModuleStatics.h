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
	static T* GetCurrentCamera(int32 InPlayerIndex = 0)
	{
		return Cast<T>(GetCurrentCamera(InPlayerIndex));
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "CameraModuleStatics")
	static ACameraActorBase* GetCurrentCamera(int32 InPlayerIndex = 0, TSubclassOf<ACameraActorBase> InClass = nullptr);
	
	UFUNCTION(BlueprintPure, Category = "CameraModuleStatics")
	static ACameraPointBase* GetDefaultCameraPoint();

	UFUNCTION(BlueprintPure, Category = "CameraModuleStatics")
	static FVector GetCameraLocation(bool bReally = false, bool bRefresh = false, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintPure, Category = "CameraModuleStatics")
	static FRotator GetCameraRotation(bool bReally = false, bool bRefresh = false, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintPure, Category = "CameraModuleStatics")
	static float GetCameraDistance(bool bReally = false, bool bRefresh = false, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintPure, Category = "CameraModuleStatics")
	static FVector GetCameraOffset(bool bReally = false, bool bRefresh = false, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintPure, Category = "CameraModuleStatics")
	static float GetCameraFov(bool bReally = false, bool bRefresh = false, int32 InPlayerIndex = 0);

	template<class T>
	static T* GetCameraByClass(int32 InPlayerIndex = 0, TSubclassOf<ACameraActorBase> InClass = T::StaticClass())
	{
		return Cast<T>(GetCameraByClass(InClass, InPlayerIndex));
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "CameraModuleStatics")
	static ACameraActorBase* GetCameraByClass(TSubclassOf<ACameraActorBase> InClass, int32 InPlayerIndex = 0);

	template<class T>
	static T* GetCameraByName(const FName InName, int32 InPlayerIndex = 0)
	{
		return Cast<T>(GetCameraByName(InName, InPlayerIndex));
	}

	UFUNCTION(BlueprintPure, Category = "CameraModuleStatics")
	static ACameraActorBase* GetCameraByName(const FName InName, int32 InPlayerIndex = 0);

	template<class T>
	static void SwitchCameraByClass(bool bReset = true, bool bInstant = false, int32 InPlayerIndex = 0, TSubclassOf<ACameraActorBase> InClass = T::StaticClass())
	{
		SwitchCameraByClass(InClass, bReset, bInstant, InPlayerIndex);
	}

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void SwitchCameraByClass(TSubclassOf<ACameraActorBase> InClass, bool bReset = true, bool bInstant = false, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void SwitchCameraByName(const FName InName, bool bReset = true, bool bInstant = false, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void SwitchCameraPoint(ACameraPointBase* InCameraPoint, bool bSetAsDefault = false, bool bInstant = false, int32 InPlayerIndex = 0);

public:
	UFUNCTION(BlueprintCallable, meta = (AdvancedDisplay = "bAllowControl,InViewEaseType,InViewDuration,bInstant"), Category = "CameraModuleStatics")
	static void StartTrackTarget(AActor* InTargetActor, ECameraTrackMode InTrackMode = ECameraTrackMode::LocationAndRotationAndDistanceOnce, ECameraViewMode InViewMode = ECameraViewMode::Smooth, ECameraViewSpace InViewSpace = ECameraViewSpace::Local, FVector InLocation = FVector(-1.f), FVector InSocketOffset = FVector(-1.f), float InYaw = -1.f, float InPitch = -1.f, float InDistance = -1.f, bool bAllowControl = true, EEaseType InViewEaseType = EEaseType::Linear, float InViewDuration = 1.f, bool bInstant = false, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void EndTrackTarget(AActor* InTargetActor = nullptr, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InCameraViewData"), Category = "CameraModuleStatics")
	static void SetCameraView(const FCameraViewData& InCameraViewData, bool bCacheData = true, bool bInstant = false, int32 InPlayerIndex = 0);
	
	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void ResetCameraView(ECameraResetMode InCameraResetMode = ECameraResetMode::DefaultPoint, bool bInstant = false, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void SetCameraLocation(FVector InLocation, bool bInstant = false, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void DoCameraLocation(FVector InLocation, float InDuration = 1.f, EEaseType InEaseType = EEaseType::Linear, bool bForce = true, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void StopDoCameraLocation(int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void SetCameraOffset(FVector InOffset = FVector(-1.f), bool bInstant = false, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void DoCameraOffset(FVector InOffset = FVector(-1.f), float InDuration = 1.f, EEaseType InEaseType = EEaseType::Linear, bool bForce = true, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void StopDoCameraOffset(int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void SetCameraRotation(float InYaw = -1.f, float InPitch = -1.f, bool bInstant = false, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void DoCameraRotation(float InYaw = -1.f, float InPitch = -1.f, float InDuration = 1.f, EEaseType InEaseType = EEaseType::Linear, bool bForce = true, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void StopDoCameraRotation(int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void SetCameraDistance(float InDistance = -1.f, bool bInstant = false, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void DoCameraDistance(float InDistance = -1.f, float InDuration = 1.f, EEaseType InEaseType = EEaseType::Linear, bool bForce = true, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void StopDoCameraDistance(int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void SetCameraRotationAndDistance(float InYaw = -1.f, float InPitch = -1.f, float InDistance = -1.f, bool bInstant = false, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void DoCameraRotationAndDistance(float InYaw = -1.f, float InPitch = -1.f, float InDistance = -1.f, float InDuration = 1.f, EEaseType InEaseType = EEaseType::Linear, bool bForce = true, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void SetCameraTransform(FVector InLocation, float InYaw = -1.f, float InPitch = -1.f, float InDistance = -1.f, bool bInstant = false, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void DoCameraTransform(FVector InLocation, float InYaw = -1.f, float InPitch = -1.f, float InDistance = -1.f, float InDuration = 1.f, EEaseType InEaseType = EEaseType::Linear, bool bForce = true, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void StopDoCameraTransform(int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void SetCameraFov(float InFov = -1.f, bool bInstant = false, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void DoCameraFov(float InFov = -1.f, float InDuration = 1.f, EEaseType InEaseType = EEaseType::Linear, bool bForce = true, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void StopDoCameraFov(int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void AddCameraMovementInput(FVector InDirection, float InValue, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void AddCameraRotationInput(float InYaw, float InPitch, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleStatics")
	static void AddCameraDistanceInput(float InValue, int32 InPlayerIndex = 0);
};
