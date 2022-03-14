// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "CameraModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
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
	static T* GetCamera(TSubclassOf<ACameraPawnBase> InClass = T::StaticClass())
	{
		return Cast<T>(K2_GetCamera(InClass));
	}

	UFUNCTION(BlueprintCallable, DisplayName = "GetCamera", meta = (DeterminesOutputType = "InClass"), Category = "CameraModuleBPLibrary")
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
};
