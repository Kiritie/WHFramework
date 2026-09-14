// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "CameraModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CameraModuleStatics.generated.h"

class ACameraPointBase;
class USpringArmComponent;
class UCameraComponent;
class ACameraActorBase;
class ACameraManagerBase;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UCameraModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "CameraModule")
	static ACameraManagerBase* GetCameraManager(int32 InPlayerIndex = 0);

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

	UFUNCTION(BlueprintCallable, Category = "CameraModule")
	static void BindTarget(const FCameraTargetRequest& InRequest, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "CameraModule")
	static void ClearTarget(AActor* InExpectedTarget = nullptr, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "CameraModule")
	static void ApplyView(const FCameraViewRequest& InRequest, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "CameraModule")
	static void ResetView(ECameraResetMode InMode = ECameraResetMode::DefaultPoint, bool bInstant = false, int32 InPlayerIndex = 0);
};
