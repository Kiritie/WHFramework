// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "CameraModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Math/MathTypes.h"
#include "CameraModuleBPLibrary.generated.h"

class USpringArmComponent;
class UCameraComponent;
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
	static T* GetCurrentCamera()
	{
		return Cast<T>(GetCurrentCamera());
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "CameraModuleBPLibrary")
	static ACameraPawnBase* GetCurrentCamera(TSubclassOf<ACameraPawnBase> InClass = nullptr);

	UFUNCTION(BlueprintPure, Category = "CameraModuleBPLibrary")
	static FVector GetCameraLocation(bool bReal = false);

	UFUNCTION(BlueprintPure, Category = "CameraModuleBPLibrary")
	static FRotator GetCameraRotation(bool bReal = false);

	UFUNCTION(BlueprintPure, Category = "CameraModuleBPLibrary")
	static float GetCameraDistance(bool bReal = false);

	template<class T>
	static T* GetCameraByClass(TSubclassOf<ACameraPawnBase> InClass = T::StaticClass())
	{
		return Cast<T>(GetCameraByClass(InClass));
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "CameraModuleBPLibrary")
	static ACameraPawnBase* GetCameraByClass(TSubclassOf<ACameraPawnBase> InClass);

	template<class T>
	static T* GetCameraByName(const FName InName)
	{
		return Cast<T>(GetCameraByName(InName));
	}

	UFUNCTION(BlueprintPure, Category = "CameraModuleBPLibrary")
	static ACameraPawnBase* GetCameraByName(const FName InName);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleBPLibrary")
	static void SwitchCamera(ACameraPawnBase* InCamera, bool bInstant = false);

	template<class T>
	static void SwitchCameraByClass(bool bInstant = false, TSubclassOf<ACameraPawnBase> InClass = T::StaticClass())
	{
		SwitchCameraByClass(InClass);
	}

	UFUNCTION(BlueprintCallable, Category = "CameraModuleBPLibrary")
	static void SwitchCameraByClass(TSubclassOf<ACameraPawnBase> InClass, bool bInstant = false);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleBPLibrary")
	static void SwitchCameraByName(const FName InName, bool bInstant = false);

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

	UFUNCTION(BlueprintCallable, Category = "CameraModuleBPLibrary")
	static void AddCameraMovementInput(FVector InDirection, float InValue);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleBPLibrary")
	static void AddCameraRotationInput(float InYaw, float InPitch);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleBPLibrary")
	static void AddCameraDistanceInput(float InValue);
};
