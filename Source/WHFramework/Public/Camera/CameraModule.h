// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "CameraModuleTypes.h"
#include "Debug/DebuggerInterface.h"
#include "Main/Base/ModuleBase.h"
#include "SaveGame/Base/SaveDataAgentInterface.h"

#include "CameraModule.generated.h"

struct FEventSwitchCameraPoint;
class ACameraPointBase;
struct FEventResetCameraView;
struct FEventSetCameraView;
class UCameraComponent;
class USpringArmComponent;
class AWHPlayerController;
class ACameraActorBase;
class ACameraManagerBase;

UCLASS()
class WHFRAMEWORK_API UCameraModule : public UModuleBase, public IDebuggerInterface
{
	GENERATED_BODY()
	
	GENERATED_MODULE(UCameraModule)

public:
	// ParamSets default values for this actor's properties
	UCameraModule();

	~UCameraModule();

	//////////////////////////////////////////////////////////////////////////
	/// ModuleBase
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
#endif

	virtual void OnInitialize() override;

	virtual void OnPreparatory(EPhase InPhase) override;

	virtual void OnRefresh(float DeltaSeconds, bool bInEditor) override;

	virtual void OnPause() override;

	virtual void OnUnPause() override;

	virtual void OnTermination(EPhase InPhase) override;

protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual void UnloadData(EPhase InPhase) override;

	virtual FSaveData* ToData() override;

public:
	virtual FString GetModuleDebugMessage() override;

	//////////////////////////////////////////////////////////////////////////
	/// Debugger
protected:
	virtual void OnDrawDebug(UCanvas* InCanvas, APlayerController* InPC) override;

	//////////////////////////////////////////////////////////////////////////
	/// Camera
protected:
	UPROPERTY(EditAnywhere, Category = "Camera")
	TArray<TSubclassOf<ACameraActorBase>> CameraClasses;
	
	UPROPERTY(EditAnywhere, Category = "Camera")
	TArray<ACameraActorBase*> Cameras;
	
	UPROPERTY(EditAnywhere, Category = "Camera")
	ACameraActorBase* DefaultCamera;

	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "DefaultCamera != nullptr"), Category = "Camera")
	bool bDefaultInstantSwitch;
		
	UPROPERTY(EditAnywhere, Category = "Camera")
	ACameraPointBase* DefaultCameraPoint;
	
	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TArray<ACameraManagerBase*> CameraManagers;

	UPROPERTY(VisibleAnywhere, Category = "Camera|Settings")
	FCameraSettings CameraSettings;

public:
	UFUNCTION(BlueprintPure)
	ACameraManagerBase* GetCameraManager(int32 InPlayerIndex = 0) const;
	
	UFUNCTION(BlueprintPure)
	ACameraPointBase* GetDefaultCameraPoint() const { return DefaultCameraPoint; }

	template<class T>
	T* GetCameraByClass(int32 InPlayerIndex = 0, TSubclassOf<ACameraActorBase> InClass = T::StaticClass())
	{
		return Cast<T>(GetCameraByClass(InClass, InPlayerIndex));
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass"))
	ACameraActorBase* GetCameraByClass(TSubclassOf<ACameraActorBase> InClass, int32 InPlayerIndex = 0);

	template<class T>
	T* GetCameraByName(const FName InName, int32 InPlayerIndex = 0)
	{
		return Cast<T>(GetCameraByName(InName, InPlayerIndex));
	}

	UFUNCTION(BlueprintPure)
	ACameraActorBase* GetCameraByName(const FName InName, int32 InPlayerIndex = 0) const;

	template<class T>
	void SwitchCameraByClass(bool bReset = true, bool bInstant = false, int32 InPlayerIndex = 0, TSubclassOf<ACameraActorBase> InClass = T::StaticClass())
	{
		SwitchCameraByClass(InClass, bReset, bInstant, InPlayerIndex);
	}

	UFUNCTION(BlueprintCallable)
	void SwitchCameraByClass(TSubclassOf<ACameraActorBase> InClass, bool bReset = true, bool bInstant = false, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable)
	void SwitchCameraByName(const FName InName, bool bReset = true, bool bInstant = false, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable)
	void SwitchCameraPoint(ACameraPointBase* InCameraPoint, bool bSetAsDefault = false, bool bInstant = false, int32 InPlayerIndex = 0);

public:
	UFUNCTION(BlueprintPure)
	const FCameraSettings& GetSettings() const { return CameraSettings; }

	UFUNCTION(BlueprintCallable)
	void ApplySettings(const FCameraSettings& InSettings);

	UFUNCTION(BlueprintCallable)
	void RegisterCameraManager(ACameraManagerBase* InCameraManager);

	UFUNCTION(BlueprintCallable)
	void UnRegisterCameraManager(ACameraManagerBase* InCameraManager);

	//////////////////////////////////////////////////////////////////////////
	/// Camera Control
protected:
	UPROPERTY(EditAnywhere, Category = "CameraControl")
	bool bCameraControlAble;

	// Move
	UPROPERTY(EditAnywhere, Category = "CameraControl|Move")
	bool bCameraMoveAble;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Move")
	bool bCameraMoveControlAble;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Move")
	FBox CameraMoveRange;

	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = EDC_DrawCameraRange), Category = "CameraControl|Move")
	bool bDrawCameraRange;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Move")
	float CameraMoveAltitude;

	// Rotate
	UPROPERTY(EditAnywhere, Category = "CameraControl|Rotate")
	bool bCameraRotateAble;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Rotate")
	bool bCameraRotateControlAble;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Rotate")
	float MinCameraPitch;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Rotate")
	float MaxCameraPitch;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Rotate")
	float InitCameraPitch;

	// Zoom
	UPROPERTY(EditAnywhere, Category = "CameraControl|Zoom")
	bool bCameraZoomAble;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Zoom")
	bool bCameraZoomControlAble;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Zoom")
	bool bCameraZoomMoveAble;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Zoom")
	bool bNormalizeCameraZoom;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Zoom")
	float CameraZoomAltitude;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Zoom")
	float InitCameraDistance;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Zoom")
	float MinCameraDistance;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Zoom")
	float MaxCameraDistance;

	// Fov
	UPROPERTY(EditAnywhere, Category = "CameraControl|Fov")
	float InitCameraFov;

	// Offset
	UPROPERTY(EditAnywhere, Category = "CameraControl|Offset")
	bool bCameraOffsetAble;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Offset")
	bool bSmoothCameraOffset;

	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "bSmoothCameraOffset == true"), Category = "CameraControl|Offset")
	float CameraOffsetSpeed;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Offset")
	FVector InitCameraOffset;

protected:
	UFUNCTION()
	void OnSetCameraView(UObject* InSender, const FEventSetCameraView& InEvent);
	
	UFUNCTION()
	void OnResetCameraView(UObject* InSender, const FEventResetCameraView& InEvent);
	
	UFUNCTION()
	void OnSwitchCameraPoint(UObject* InSender, const FEventSwitchCameraPoint& InEvent);

public:
	UFUNCTION(BlueprintPure)
	bool IsCameraControlAble() const { return bCameraControlAble; }

	UFUNCTION(BlueprintCallable)
	void SetCameraControlAble(bool bInCameraControlAble) { bCameraControlAble = bInCameraControlAble; }

	UFUNCTION(BlueprintCallable)
	void SetCameraMovementAble(bool bInCameraMoveAble, bool bInCameraRotateAble, bool bInCameraZoomAble) { bCameraMoveAble = bInCameraMoveAble; bCameraRotateAble = bInCameraRotateAble; bCameraZoomAble = bInCameraZoomAble; }

	UFUNCTION(BlueprintCallable)
	void SetCameraMovementControlAble(bool bInCameraMoveControlAble, bool bInCameraRotateControlAble, bool bInCameraZoomControlAble) { bCameraMoveControlAble = bInCameraMoveControlAble; bCameraRotateControlAble = bInCameraRotateControlAble; bCameraZoomControlAble = bInCameraZoomControlAble; }

	UFUNCTION(BlueprintPure)
	bool IsCameraMoveAble() const { return bCameraMoveAble; }

	UFUNCTION(BlueprintCallable)
	void SetCameraMoveAble(bool bInCameraMoveAble) { bCameraMoveAble = bInCameraMoveAble; }

	UFUNCTION(BlueprintPure)
	bool IsCameraMoveControlAble() const { return bCameraMoveControlAble; }

	UFUNCTION(BlueprintCallable)
	void SetCameraMoveControlAble(bool bInCameraMoveControlAble) { bCameraMoveControlAble = bInCameraMoveControlAble; }

	UFUNCTION(BlueprintPure)
	bool IsClampCameraMove() const { return CameraMoveRange.IsValid != 0; }

	UFUNCTION(BlueprintCallable)
	void SetClampCameraMove(bool bInClampCameraMove) { CameraMoveRange.IsValid = bInClampCameraMove; }

	UFUNCTION(BlueprintPure)
	FBox GetCameraMoveRange() const { return CameraMoveRange; }

	UFUNCTION(BlueprintCallable)
	void SetCameraMoveRange(const FBox InCameraMoveRange) { CameraMoveRange = InCameraMoveRange; }

	UFUNCTION(BlueprintPure)
	float GetCameraMoveAltitude() const { return CameraMoveAltitude; }

	UFUNCTION(BlueprintPure)
	bool IsCameraRotateAble() const { return bCameraRotateAble; }

	UFUNCTION(BlueprintCallable)
	void SetCameraRotateAble(bool bInCameraRotateAble) { bCameraRotateAble = bInCameraRotateAble; }

	UFUNCTION(BlueprintPure)
	bool IsCameraRotateControlAble() const { return bCameraRotateControlAble; }

	UFUNCTION(BlueprintCallable)
	void SetCameraRotateControlAble(bool bInCameraRotateControlAble) { bCameraRotateControlAble = bInCameraRotateControlAble; }

	UFUNCTION(BlueprintPure)
	float GetMinCameraPitch() const{ return MinCameraPitch; }

	UFUNCTION(BlueprintCallable)
	void SetMinCameraPitch(float InMinCameraPitch) { MinCameraPitch = InMinCameraPitch; }

	UFUNCTION(BlueprintPure)
	float GetMaxCameraPitch() const{ return MaxCameraPitch; }

	UFUNCTION(BlueprintCallable)
	void SetMaxCameraPitch(float InMaxCameraPitch) { MaxCameraPitch = InMaxCameraPitch; }

	UFUNCTION(BlueprintPure)
	float GetInitCameraPitch() const { return InitCameraPitch; }

	UFUNCTION(BlueprintCallable)
	void SetInitCameraPitch(float InInitCameraPitch) { InitCameraPitch = InInitCameraPitch; }

	UFUNCTION(BlueprintPure)
	bool IsCameraZoomAble() const { return bCameraZoomAble; }

	UFUNCTION(BlueprintCallable)
	void SetCameraZoomAble(bool bInCameraZoomAble) { bCameraZoomAble = bInCameraZoomAble; }
	
	UFUNCTION(BlueprintPure)
	bool IsCameraZoomControlAble() const { return bCameraZoomControlAble; }

	UFUNCTION(BlueprintCallable)
	void SetCameraZoomControlAble(bool bInCameraZoomControlAble) { bCameraZoomControlAble = bInCameraZoomControlAble; }

	UFUNCTION(BlueprintPure)
	bool IsCameraZoomMoveAble() const { return bCameraZoomMoveAble; }

	UFUNCTION(BlueprintCallable)
	void SetCameraZoomMoveAble(bool bInCameraZoomMoveAble) { bCameraZoomMoveAble = bInCameraZoomMoveAble; }

	UFUNCTION(BlueprintPure)
	bool IsNormalizeCameraZoom() const { return bNormalizeCameraZoom; }

	UFUNCTION(BlueprintPure)
	float GetCameraZoomAltitude() const { return CameraZoomAltitude; }

	UFUNCTION(BlueprintPure)
	float GetMinCameraDistance() const { return MinCameraDistance; }

	UFUNCTION(BlueprintCallable)
	void SetMinCameraDistance(float InMinCameraDistance) { MinCameraDistance = InMinCameraDistance; }

	UFUNCTION(BlueprintPure)
	float GetMaxCameraDistance() const { return MaxCameraDistance; }

	UFUNCTION(BlueprintCallable)
	void SetMaxCameraDistance(float InMaxCameraDistance) { MaxCameraDistance = InMaxCameraDistance; }

	UFUNCTION(BlueprintPure)
	float GetInitCameraDistance() const { return InitCameraDistance; }

	UFUNCTION(BlueprintCallable)
	void SetInitCameraDistance(float InInitCameraDistance) { InitCameraDistance = InInitCameraDistance; }
	
	UFUNCTION(BlueprintPure)
	bool IsCameraOffsetAble() const { return bCameraOffsetAble; }

	UFUNCTION(BlueprintCallable)
	void SetCameraOffsetAble(bool bInCameraOffsetAble) { bCameraOffsetAble = bInCameraOffsetAble; }

	UFUNCTION(BlueprintPure)
	bool IsSmoothCameraOffset() const { return bSmoothCameraOffset; }

	UFUNCTION(BlueprintCallable)
	void SetSmoothCameraOffset(bool bInSmoothCameraOffset) { bSmoothCameraOffset = bInSmoothCameraOffset; }

	UFUNCTION(BlueprintPure)
	float GetCameraOffsetSpeed() const { return CameraOffsetSpeed; }

	UFUNCTION(BlueprintCallable)
	void SetCameraOffsetSpeed(float InCameraOffsetSpeed) { CameraOffsetSpeed = InCameraOffsetSpeed; }

	UFUNCTION(BlueprintPure)
	FVector GetInitCameraOffset() const { return InitCameraOffset; }

	UFUNCTION(BlueprintPure)
	float GetInitCameraFov() const { return InitCameraFov; }

	//////////////////////////////////////////////////////////////////////////
	/// Network
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	bool EDC_DrawCameraRange() const { return CameraMoveRange.IsValid == 1; };
};
