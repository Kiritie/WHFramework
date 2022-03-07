// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "CameraModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CameraModuleBPLibrary.generated.h"

class ACameraModule;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UCameraModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "CameraModuleBPLibrary")
	static void AddCameraToList(class ACameraPawnBase* InCamera);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleBPLibrary")
	static void RemoveCameraFromList(class ACameraPawnBase* InCamera);

	UFUNCTION(BlueprintCallable, Category = "CameraModuleBPLibrary")
	static void RemoveCameraFromListByName(const FName InCameraName);

	UFUNCTION(BlueprintPure, Category = "CameraModuleBPLibrary")
	static class ACameraPawnBase* GetCameraByName(const FName InCameraName);
};
