#pragma once
#include "Camera/Mode/CameraModeBase.h"
#include "Camera/CameraModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CameraModuleStatics.generated.h"
class ACameraManagerBase;
class UCameraDirectorBase;
class UCameraFeatureBase;
class UCameraModeBase;
UCLASS()
class WHFRAMEWORK_API UCameraModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category = "CameraModule")
	static ACameraManagerBase* GetCameraManager(int32 InPlayerIndex = 0);
	UFUNCTION(BlueprintPure, Category = "CameraModule")
	static FCameraViewSnapshot GetViewSnapshot(int32 InPlayerIndex = 0);
	UFUNCTION(BlueprintPure, Category = "CameraModule")
	static FTransform GetViewTransform(int32 InPlayerIndex = 0);
	UFUNCTION(BlueprintPure, Category = "CameraModule")
	static FVector GetViewLocation(int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintPure, Category = "CameraModule")
	static FRotator GetViewRotation(int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintPure, Category = "CameraModule")
	static float GetViewFOV(int32 InPlayerIndex = 0);
	UFUNCTION(BlueprintCallable, Category = "CameraModule")
	static void SetCameraMode(TSubclassOf<UCameraModeBase> ModeClass, const FCameraModeContext& Context, int32 InPlayerIndex = 0);
	UFUNCTION(BlueprintCallable, Category = "CameraModule")
	static FCameraFeatureHandle PushFeature(TSubclassOf<UCameraFeatureBase> FeatureClass, const FCameraFeatureContext& Context, int32 InPlayerIndex = 0);
	UFUNCTION(BlueprintCallable, Category = "CameraModule")
	static void PopFeature(FCameraFeatureHandle Handle, int32 InPlayerIndex = 0);
	UFUNCTION(BlueprintCallable, Category = "CameraModule")
	static void StartDirector(TSubclassOf<UCameraDirectorBase> DirectorClass, int32 InPlayerIndex = 0);
	UFUNCTION(BlueprintCallable, Category = "CameraModule")
	static void StopDirector(int32 InPlayerIndex = 0);
};
