#pragma once

#include "Camera/CameraModuleTypes.h"
#include "Camera/PlayerCameraManager.h"
#include "CameraManagerBase.generated.h"

class ACameraRigBase;
class UCameraDirectorBase;
class UCameraFeatureBase;
class UCameraModeBase;

UCLASS(NotPlaceable)
class WHFRAMEWORK_API ACameraManagerBase : public APlayerCameraManager
{
	GENERATED_BODY()

	friend class UCameraModule;

public:
	ACameraManagerBase(const FObjectInitializer& ObjectInitializer);

	virtual void InitializeFor(APlayerController* PC) override;

	virtual void Destroyed() override;

	virtual void SetViewTarget(AActor* NewViewTarget, FViewTargetTransitionParams TransitionParams) override;

	virtual void UpdateCamera(float DeltaTime) override;

	//////////////////////////////////////////////////////////////////////////
	/// Camera Mode
public:
	UFUNCTION(BlueprintCallable, Category = "CameraModule")
	void SetMode(TSubclassOf<UCameraModeBase> ModeClass, const FCameraModeContext& Context);

	UFUNCTION(BlueprintCallable, Category = "CameraModule")
	void SetDefaultMode(const FCameraTransitionParams& Transition);

	UFUNCTION(BlueprintCallable, Category = "CameraModule")
	void ClearModeTarget();

	UFUNCTION(BlueprintCallable, Category = "CameraModule")
	void AddInputIntent(const FCameraInputIntent& Intent);

	UFUNCTION(BlueprintCallable, Category = "CameraModule")
	FCameraConfigOverrideHandle PushCameraConfigOverride(const FCameraConfigOverride& InOverride, int32 InPriority = 0);

	UFUNCTION(BlueprintCallable, Category = "CameraModule")
	void PopCameraConfigOverride(FCameraConfigOverrideHandle InHandle);

	UFUNCTION(BlueprintCallable, Category = "CameraModule")
	void ClearCameraConfigOverrides();

	const FCameraResolvedConfig& GetResolvedCameraConfig();

	//////////////////////////////////////////////////////////////////////////
	/// Camera Feature
public:
	UFUNCTION(BlueprintCallable, Category = "CameraModule")
	FCameraFeatureHandle PushFeature(TSubclassOf<UCameraFeatureBase> FeatureClass, const FCameraFeatureContext& Context);

	UFUNCTION(BlueprintCallable, Category = "CameraModule")
	void PopFeature(FCameraFeatureHandle Handle);

	//////////////////////////////////////////////////////////////////////////
	/// Camera Director
public:
	UFUNCTION(BlueprintCallable, Category = "CameraModule")
	void StartDirector(TSubclassOf<UCameraDirectorBase> DirectorClass);

	UFUNCTION(BlueprintCallable, Category = "CameraModule")
	void StopDirector();

	//////////////////////////////////////////////////////////////////////////
	/// Camera State
public:
	void ApplyUserSettings(const FCameraUserSettings& InSettings) { UserSettings = InSettings; }

	void SetInitialView(const FRotator& Rotation, float Distance);

	void SetDesiredPivot(const FVector& Location, const FRotator& Rotation);

	UFUNCTION(BlueprintPure, Category = "CameraModule")
	const FCameraViewSnapshot& GetFinalView() const { return FinalView; }

	UFUNCTION(BlueprintPure, Category = "CameraModule")
	float GetCurrentRigDistance() const { return DesiredState.ArmLength; }

	UFUNCTION(BlueprintPure, Category = "CameraModule")
	int32 GetLocalPlayerIndex() const { return LocalPlayerIndex; }

	ACameraRigBase* GetRuntimeRig() const { return RuntimeRig; }

	//////////////////////////////////////////////////////////////////////////
protected:
	UPROPERTY(Transient)
	TObjectPtr<ACameraRigBase> RuntimeRig;

	UPROPERTY(Transient)
	TObjectPtr<UCameraModeBase> ActiveMode;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UCameraFeatureBase>> ActiveFeatures;

	UPROPERTY(Transient)
	TObjectPtr<UCameraDirectorBase> ActiveDirector;

	UPROPERTY(Transient)
	FCameraRigState DesiredState;

	UPROPERTY(Transient)
	FCameraViewSnapshot FinalView;

	UPROPERTY(Transient)
	FCameraUserSettings UserSettings;

	UPROPERTY(Transient)
	TArray<FCameraConfigOverrideEntry> CameraConfigOverrides;

	UPROPERTY(Transient)
	int32 LocalPlayerIndex = INDEX_NONE;

private:
	FCameraRigState TransitionSource;

	FCameraTransitionParams ActiveTransition;

	float TransitionElapsed = 0.f;

	bool bTransitionActive = false;

	uint64 CameraOverrideSequence = 0;

	FCameraResolvedConfig CachedResolvedCameraConfig;

	bool bResolvedCameraConfigDirty = true;

	float CurrentArmLength = 0.f;

	void SpawnRuntimeRig();

	void ActivateDefaultMode();

	void ApplyRig(float DeltaTime);

	void RefreshFinalView();

	FCameraInputIntent ResolveInput(const FCameraInputIntent& InRawInput) const;

	FCameraResolvedConfig ResolveCameraConfig(const FCameraTargetParams* InTargetParams) const;

	const FCameraTargetParams* GetActiveTargetParams() const;

	static float ResolveSmoothSpeed(float InSmoothingAmount, float InMinSmoothSpeed, float InMaxSmoothSpeed);
};
