// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "CameraModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Math/MathTypes.h"
#include "CameraModuleBPLibrary.generated.h"

class ACameraPawnBase;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UCameraModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	template<class T>
	static T* GetCurrentCamera(TSubclassOf<ACameraPawnBase> InClass = T::StaticClass())
	{
		return Cast<T>(K2_GetCurrentCamera(InClass));
	}

	UFUNCTION(BlueprintPure, DisplayName = "GetCamera", meta = (DeterminesOutputType = "InClass"), Category = "CameraModuleBPLibrary")
	static ACameraPawnBase* K2_GetCurrentCamera(TSubclassOf<ACameraPawnBase> InClass);
	
	template<class T>
	static T* GetCamera(TSubclassOf<ACameraPawnBase> InClass = T::StaticClass())
	{
		return Cast<T>(K2_GetCamera(InClass));
	}

	UFUNCTION(BlueprintPure, DisplayName = "GetCamera", meta = (DeterminesOutputType = "InClass"), Category = "CameraModuleBPLibrary")
	static ACameraPawnBase* K2_GetCamera(TSubclassOf<ACameraPawnBase> InClass);

	template<class T>
	static T* GetCameraByName(const FName InCameraName)
	{
		return Cast<T>(K2_GetCameraByName(InCameraName));
	}

	UFUNCTION(BlueprintPure, DisplayName = "GetCamera", Category = "CameraModuleBPLibrary")
	static ACameraPawnBase* K2_GetCameraByName(const FName InCameraName);

	template<class T>
	static void SwitchCamera(TSubclassOf<ACameraPawnBase> InClass = T::StaticClass())
	{
		K2_SwitchCamera(InClass);
	}

	UFUNCTION(BlueprintCallable, DisplayName = "SwitchCamera", Category = "CameraModuleBPLibrary")
	static void K2_SwitchCamera(TSubclassOf<ACameraPawnBase> InClass);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleBPLibrary")
	static void SwitchCameraByName(const FName InCameraName);

public:
	UFUNCTION(BlueprintCallable, Category = "CameraModuleBPLibrary")
	static void StartTrackTarget(AActor* InTargetActor, ETrackTargetMode InTrackTargetMode = ETrackTargetMode::LocationAndRotationAndDistanceOnce, ETrackTargetSpace InTrackTargetSpace = ETrackTargetSpace::Local, FVector InLocationOffset = FVector(-1.f), float InYawOffset = -1.f, float InPitchOffset = -1.f, float InDistance = -1.f, bool bAllowControl = true, bool bInstant = false);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleBPLibrary")
	static void EndTrackTarget(AActor* InTargetActor = nullptr);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleBPLibrary")
	static void SetCameraLocation(FVector InLocation, bool bInstant = false);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleBPLibrary")
	static void DoCameraLocation(FVector InLocation, float InDuration = 1.f, EEaseType InEaseType = EEaseType::Linear);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleBPLibrary")
	static void StopDoCameraLocation();

	UFUNCTION(BlueprintCallable, Category = "CameraModuleBPLibrary")
	static void SetCameraRotation(float InYaw = -1.f, float InPitch = -1.f, bool bInstant = false);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleBPLibrary")
	static void DoCameraRotation(float InYaw = -1.f, float InPitch = -1.f, float InDuration = 1.f, EEaseType InEaseType = EEaseType::Linear);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleBPLibrary")
	static void StopDoCameraRotation();

	UFUNCTION(BlueprintCallable, Category = "CameraModuleBPLibrary")
	static void SetCameraDistance(float InDistance = -1.f, bool bInstant = false);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleBPLibrary")
	static void DoCameraDistance(float InDistance = -1.f, float InDuration = 1.f, EEaseType InEaseType = EEaseType::Linear);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleBPLibrary")
	static void StopDoCameraDistance();

	UFUNCTION(BlueprintCallable, Category = "CameraModuleBPLibrary")
	static void SetCameraRotationAndDistance(float InYaw = -1.f, float InPitch = -1.f, float InDistance = -1.f, bool bInstant = false);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleBPLibrary")
	static void DoCameraRotationAndDistance(float InYaw = -1.f, float InPitch = -1.f, float InDistance = -1.f, float InDuration = 1.f, EEaseType InEaseType = EEaseType::Linear);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleBPLibrary")
	static void SetCameraTransform(FVector InLocation, float InYaw = -1.f, float InPitch = -1.f, float InDistance = -1.f, bool bInstant = false);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleBPLibrary")
	static void DoCameraTransform(FVector InLocation, float InYaw = -1.f, float InPitch = -1.f, float InDistance = -1.f, float InDuration = 1.f, EEaseType InEaseType = EEaseType::Linear);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleBPLibrary")
	static void StopDoCameraTransform();
};
