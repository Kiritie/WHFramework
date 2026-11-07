#pragma once

#include "Camera/Mode/CameraModeBase.h"
#include "Camera/CameraViewTypes.h"
#include "Camera/CameraFeatureTypes.h"
#include "Camera/CameraModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CameraModuleStatics.generated.h"

class ACameraManagerBase;
class APlayerController;
class UCameraDirectorBase;
class UCameraFeatureBase;
class UCameraModeBase;

UCLASS()
class WHFRAMEWORK_API UCameraModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category="CameraModule")
	static ACameraManagerBase* GetCameraManager(APlayerController* InPlayer);
	UFUNCTION(BlueprintPure, Category="CameraModule")
	static FCameraViewSnapshot GetViewSnapshot(APlayerController* InPlayer);
	UFUNCTION(BlueprintPure, Category="CameraModule")
	static FTransform GetViewTransform(APlayerController* InPlayer);
	UFUNCTION(BlueprintPure, Category="CameraModule")
	static FVector GetViewLocation(APlayerController* InPlayer);
	UFUNCTION(BlueprintPure, Category="CameraModule")
	static FRotator GetViewRotation(APlayerController* InPlayer);
	UFUNCTION(BlueprintPure, Category="CameraModule")
	static float GetViewFOV(APlayerController* InPlayer);
	UFUNCTION(BlueprintCallable, Category="CameraModule")
	static void SetCameraMode(APlayerController* InPlayer, TSubclassOf<UCameraModeBase> ModeClass, const FCameraModeContext& Context);
	UFUNCTION(BlueprintCallable, Category="CameraModule")
	static FCameraFeatureHandle PushFeature(APlayerController* InPlayer, TSubclassOf<UCameraFeatureBase> FeatureClass, const FCameraFeatureContext& Context);
	UFUNCTION(BlueprintCallable, Category="CameraModule")
	static void PopFeature(APlayerController* InPlayer, FCameraFeatureHandle Handle);
	UFUNCTION(BlueprintCallable, Category="CameraModule")
	static void StartDirector(APlayerController* InPlayer, TSubclassOf<UCameraDirectorBase> DirectorClass);
	UFUNCTION(BlueprintCallable, Category="CameraModule")
	static void StopDirector(APlayerController* InPlayer);
};
