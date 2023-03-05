// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"

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

	static ACameraPawnBase* GetCurrentCamera();

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InCameraClass"), Category = "CameraModuleBPLibrary")
	static ACameraPawnBase* GetCurrentCamera(TSubclassOf<ACameraPawnBase> InCameraClass);
	
	UFUNCTION(BlueprintPure, Category = "CameraModuleBPLibrary")
	static UCameraComponent* GetCurrentCameraComp();

	UFUNCTION(BlueprintPure, Category = "CameraModuleBPLibrary")
	static USpringArmComponent* GetCurrentCameraBoom();

	template<class T>
	static T* GetCameraByClass(TSubclassOf<ACameraPawnBase> InCameraClass = T::StaticClass())
	{
		return Cast<T>(GetCameraByClass(InCameraClass));
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InCameraClass"), Category = "CameraModuleBPLibrary")
	static ACameraPawnBase* GetCameraByClass(TSubclassOf<ACameraPawnBase> InCameraClass);

	template<class T>
	static T* GetCameraByName(const FName InCameraName)
	{
		return Cast<T>(GetCameraByName(InCameraName));
	}

	UFUNCTION(BlueprintPure, Category = "CameraModuleBPLibrary")
	static ACameraPawnBase* GetCameraByName(const FName InCameraName);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleBPLibrary")
	static void SwitchCamera(ACameraPawnBase* InCamera, bool bInstant = false);

	template<class T>
	static void SwitchCameraByClass(bool bInstant = false, TSubclassOf<ACameraPawnBase> InCameraClass = T::StaticClass())
	{
		SwitchCameraByClass(InCameraClass);
	}

	UFUNCTION(BlueprintCallable, Category = "CameraModuleBPLibrary")
	static void SwitchCameraByClass(TSubclassOf<ACameraPawnBase> InCameraClass, bool bInstant = false);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleBPLibrary")
	static void SwitchCameraByName(const FName InCameraName, bool bInstant = false);

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

public:
	UFUNCTION(BlueprintPure, Category = "CameraModuleBPLibrary")
	static FVector GetCurrentCameraLocation();

	UFUNCTION(BlueprintPure, Category = "CameraModuleBPLibrary")
	static FRotator GetCurrentCameraRotation();

	UFUNCTION(BlueprintPure, Category = "CameraModuleBPLibrary")
	static float GetCurrentCameraDistance();
	
	UFUNCTION(BlueprintPure, Category = "CameraModuleBPLibrary")
	static FVector GetTargetCameraLocation();

	UFUNCTION(BlueprintPure, Category = "CameraModuleBPLibrary")
	static FRotator GetTargetCameraRotation();

	UFUNCTION(BlueprintPure, Category = "CameraModuleBPLibrary")
	static float GetTargetCameraDistance();
	
	UFUNCTION(BlueprintPure, Category = "CameraModuleBPLibrary")
	static FVector GetRealCameraLocation();

	UFUNCTION(BlueprintPure, Category = "CameraModuleBPLibrary")
	static FRotator GetRealCameraRotation();

public:
	/**
	 * Set the view target blending with variable control
	 * @param NewViewTarget - new actor to set as view target
	 * @param BlendTime - time taken to blend 
	 * @param BlendFunc - Cubic, Linear etc functions for blending
	 * @param BlendExp -  Exponent, used by certain blend functions to control the shape of the curve. 
	 * @param bLockOutgoing - If true, lock outgoing viewtarget to last frame's camera position for the remainder of the blend.
	 */
	//UFUNCTION(BlueprintCallable, Category = "CameraModuleBPLibrary")
	static void SetViewTargetWithBlend(class AActor* NewViewTarget, float BlendTime = 0, enum EViewTargetBlendFunction BlendFunc = VTBlend_Linear, float BlendExp = 0, bool bLockOutgoing = false);
};
