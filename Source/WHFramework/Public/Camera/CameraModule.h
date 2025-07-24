// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "CameraModuleTypes.h"
#include "Debug/DebuggerInterface.h"
#include "Main/Base/ModuleBase.h"
#include "SaveGame/Base/SaveDataAgentInterface.h"

#include "CameraModule.generated.h"

class UEventHandle_SwitchCameraPoint;
class ACameraPointBase;
class UEventHandle_ResetCameraView;
class UEventHandle_SetCameraView;
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

public:
	UFUNCTION(BlueprintPure)
	ACameraManagerBase* GetCameraManager(int32 InPlayerIndex = 0) const;
	
	template<class T>
	T* GetCurrentCamera(int32 InPlayerIndex = 0)
	{
		return Cast<T>(GetCurrentCamera(InPlayerIndex, T::StaticClass()));
	}
	
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	ACameraActorBase* GetCurrentCamera(int32 InPlayerIndex = 0, TSubclassOf<ACameraActorBase> InClass = nullptr) const;

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
	bool bEnableCameraPanZMove;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Move")
	bool bReverseCameraPanMove;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Move")
	FBox CameraMoveRange;

	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = EDC_DrawCameraRange), Category = "CameraControl|Move")
	bool bDrawCameraRange;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Move")
	float CameraMoveRate;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Move")
	bool bSmoothCameraMove;

	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "bSmoothCameraMove == true"), Category = "CameraControl|Move")
	float CameraMoveSpeed;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Move")
	float CameraMoveAltitude;

	// Rotate
	UPROPERTY(EditAnywhere, Category = "CameraControl|Rotate")
	bool bCameraRotateAble;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Rotate")
	bool bCameraRotateControlAble;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Rotate")
	bool bReverseCameraPitch;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Rotate")
	float CameraTurnRate;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Rotate")
	float CameraLookUpRate;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Rotate")
	bool bSmoothCameraRotate;

	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "bSmoothCameraRotate == true"), Category = "CameraControl|Rotate")
	float CameraRotateSpeed;

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
	float CameraZoomRate;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Zoom")
	bool bSmoothCameraZoom;

	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "bSmoothCameraZoom == true"), Category = "CameraControl|Zoom")
	float CameraZoomSpeed;

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

public:
	UFUNCTION(BlueprintCallable, meta = (AdvancedDisplay = "bAllowControl,InViewEaseType,InViewDuration,bInstant"))
	virtual void StartTrackTarget(AActor* InTargetActor, ECameraTrackMode InTrackMode = ECameraTrackMode::LocationAndRotationAndDistanceOnce, ECameraViewMode InViewMode = ECameraViewMode::Smooth, ECameraViewSpace InViewSpace = ECameraViewSpace::Local, FVector InLocation = FVector(-1.f), FVector InOffset = FVector(-1.f), float InYaw = -1.f, float InPitch = -1.f, float InDistance = -1.f, bool bAllowControl = true, EEaseType InViewEaseType = EEaseType::Linear, float InViewDuration = 1.f, bool bInstant = false, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable)
	virtual void EndTrackTarget(AActor* InTargetActor = nullptr, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable)
	virtual void SetCameraLocation(FVector InLocation, bool bInstant = false, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable)
	virtual void DoCameraLocation(FVector InLocation, float InDuration = 1.f, EEaseType InEaseType = EEaseType::Linear, bool bForce = true, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable)
	virtual void StopDoCameraLocation(int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable)
	virtual void SetCameraOffset(FVector InOffset = FVector(-1.f), bool bInstant = false, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable)
	virtual void DoCameraOffset(FVector InOffset = FVector(-1.f), float InDuration = 1.f, EEaseType InEaseType = EEaseType::Linear, bool bForce = true, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable)
	virtual void StopDoCameraOffset(int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable)
	virtual void SetCameraRotation(float InYaw = -1.f, float InPitch = -1.f, bool bInstant = false, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable)
	virtual void DoCameraRotation(float InYaw = -1.f, float InPitch = -1.f, float InDuration = 1.f, EEaseType InEaseType = EEaseType::Linear, bool bForce = true, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable)
	virtual void StopDoCameraRotation(int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable)
	virtual void SetCameraDistance(float InDistance = -1.f, bool bInstant = false, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable)
	virtual void DoCameraDistance(float InDistance = -1.f, float InDuration = 1.f, EEaseType InEaseType = EEaseType::Linear, bool bForce = true, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable)
	virtual void StopDoCameraDistance(int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable)
	virtual void SetCameraRotationAndDistance(float InYaw = -1.f, float InPitch = -1.f, float InDistance = -1.f, bool bInstant = false, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable)
	virtual void DoCameraRotationAndDistance(float InYaw = -1.f, float InPitch = -1.f, float InDistance = -1.f, float InDuration = 1.f, EEaseType InEaseType = EEaseType::Linear, bool bForce = true, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable)
	virtual void SetCameraTransform(FVector InLocation, float InYaw = -1.f, float InPitch = -1.f, float InDistance = -1.f, bool bInstant = false, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable)
	virtual void DoCameraTransform(FVector InLocation, float InYaw = -1.f, float InPitch = -1.f, float InDistance = -1.f, float InDuration = 1.f, EEaseType InEaseType = EEaseType::Linear, bool bForce = true, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable)
	virtual void StopDoCameraTransform(int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable)
	virtual void SetCameraFov(float InFov = -1.f, bool bInstant = false, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable)
	virtual void DoCameraFov(float InFov = -1.f, float InDuration = 1.f, EEaseType InEaseType = EEaseType::Linear, bool bForce = true, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable)
	virtual void StopDoCameraFov(int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable)
	virtual void AddCameraMovementInput(FVector InDirection, float InValue, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable)
	virtual void AddCameraRotationInput(float InYaw, float InPitch, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable)
	virtual void AddCameraDistanceInput(float InValue, int32 InPlayerIndex = 0);

public:
	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InCameraViewData"))
	virtual void SetCameraView(const FCameraViewData& InCameraViewData, bool bCacheData = true, bool bInstant = false, int32 InPlayerIndex = 0);
	
	UFUNCTION(BlueprintCallable)
	virtual void ResetCameraView(ECameraResetMode InCameraResetMode = ECameraResetMode::DefaultPoint, bool bInstant = false, int32 InPlayerIndex = 0);

protected:
	UFUNCTION()
	void OnSetCameraView(UObject* InSender, UEventHandle_SetCameraView* InEventHandle);
	
	UFUNCTION()
	void OnResetCameraView(UObject* InSender, UEventHandle_ResetCameraView* InEventHandle);
	
	UFUNCTION()
	void OnSwitchCameraPoint(UObject* InSender, UEventHandle_SwitchCameraPoint* InEventHandle);

public:
	UFUNCTION(BlueprintPure)
	bool IsControllingMove(int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintPure)
	bool IsControllingRotate(int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintPure)
	bool IsControllingZoom(int32 InPlayerIndex = 0);

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
	bool IsEnableCameraPanZMove() const { return bEnableCameraPanZMove; }

	UFUNCTION(BlueprintCallable)
	void SetEnableCameraPanZMove(bool bInEnableCameraPanZMove) { bEnableCameraPanZMove = bInEnableCameraPanZMove; }

	UFUNCTION(BlueprintPure)
	bool IsReverseCameraPanMove() const { return bReverseCameraPanMove; }

	UFUNCTION(BlueprintCallable)
	void SetReverseCameraPanMove(bool bInReverseCameraPanMove) { bReverseCameraPanMove = bInReverseCameraPanMove; }

	UFUNCTION(BlueprintPure)
	bool IsClampCameraMove() const { return CameraMoveRange.IsValid != 0; }

	UFUNCTION(BlueprintCallable)
	void SetClampCameraMove(bool bInClampCameraMove) { CameraMoveRange.IsValid = bInClampCameraMove; }

	UFUNCTION(BlueprintPure)
	FBox GetCameraMoveRange() const { return CameraMoveRange; }

	UFUNCTION(BlueprintCallable)
	void SetCameraMoveRange(const FBox InCameraMoveRange) { CameraMoveRange = InCameraMoveRange; }

	UFUNCTION(BlueprintPure)
	float GetCameraMoveRate() const { return CameraMoveRate; }

	UFUNCTION(BlueprintCallable)
	void SetCameraMoveRate(float InCameraMoveRate) { CameraMoveRate = InCameraMoveRate; }

	UFUNCTION(BlueprintPure)
	bool IsSmoothCameraMove() const { return bSmoothCameraMove; }

	UFUNCTION(BlueprintCallable)
	void SetSmoothCameraMove(bool bInSmoothCameraMove) { bSmoothCameraMove = bInSmoothCameraMove; }

	UFUNCTION(BlueprintPure)
	float GetCameraMoveSpeed() const { return CameraMoveSpeed; }

	UFUNCTION(BlueprintCallable)
	void SetCameraMoveSpeed(float InCameraMoveSpeed) { CameraMoveSpeed = InCameraMoveSpeed; }

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
	bool IsReverseCameraPitch() const { return bReverseCameraPitch; }

	UFUNCTION(BlueprintCallable)
	void SetReverseCameraPitch(bool bInReverseCameraPitch) { bReverseCameraPitch = bInReverseCameraPitch; }

	UFUNCTION(BlueprintPure)
	float GetCameraTurnRate() const { return CameraTurnRate; }

	UFUNCTION(BlueprintCallable)
	void SetCameraTurnRate(float InCameraTurnRate) { CameraTurnRate = InCameraTurnRate; }

	UFUNCTION(BlueprintPure)
	float GetCameraLookUpRate() const { return CameraLookUpRate; }

	UFUNCTION(BlueprintCallable)
	void SetCameraLookUpRate(float InCameraLookUpRate) { CameraLookUpRate = InCameraLookUpRate; }

	UFUNCTION(BlueprintPure)
	bool IsSmoothCameraRotate() const { return bSmoothCameraRotate; }

	UFUNCTION(BlueprintCallable)
	void SetSmoothCameraRotate(bool bInSmoothCameraRotate) { bSmoothCameraRotate = bInSmoothCameraRotate; }

	UFUNCTION(BlueprintPure)
	float GetCameraRotateSpeed() const { return CameraRotateSpeed; }

	UFUNCTION(BlueprintCallable)
	void SetCameraRotateSpeed(float InCameraRotateSpeed) { CameraRotateSpeed = InCameraRotateSpeed; }

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
	float GetCameraZoomRate() const { return CameraZoomRate; }

	UFUNCTION(BlueprintCallable)
	void SetCameraZoomRate(float InCameraZoomRate) { CameraZoomRate = InCameraZoomRate; }

	UFUNCTION(BlueprintPure)
	bool IsNormalizeCameraZoom() const { return bNormalizeCameraZoom; }

	UFUNCTION(BlueprintPure)
	bool IsSmoothCameraZoom() const { return bSmoothCameraZoom; }

	UFUNCTION(BlueprintCallable)
	void SetSmoothCameraZoom(bool bInSmoothCameraZoom) { bSmoothCameraZoom = bInSmoothCameraZoom; }

	UFUNCTION(BlueprintPure)
	float GetCameraZoomSpeed() const { return CameraZoomSpeed; }

	UFUNCTION(BlueprintCallable)
	void SetCameraZoomSpeed(float InCameraZoomSpeed) { CameraZoomSpeed = InCameraZoomSpeed; }

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
	FVector GetRealCameraLocation(int32 InPlayerIndex = 0);
	
	UFUNCTION(BlueprintPure)
	FVector GetCurrentCameraLocation(bool bRefresh = false, int32 InPlayerIndex = 0) const;

	UFUNCTION(BlueprintPure)
	FVector GetRealCameraOffset(int32 InPlayerIndex = 0) const;

	UFUNCTION(BlueprintPure)
	FVector GetCurrentCameraOffset(bool bRefresh = false, int32 InPlayerIndex = 0) const;

	UFUNCTION(BlueprintPure)
	FRotator GetRealCameraRotation(int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintPure)
	FRotator GetCurrentCameraRotation(bool bRefresh = false, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintPure)
	float GetRealCameraDistance(int32 InPlayerIndex = 0) const;

	UFUNCTION(BlueprintPure)
	float GetCurrentCameraDistance(bool bRefresh = false, int32 InPlayerIndex = 0) const;

	UFUNCTION(BlueprintPure)
	float GetRealCameraFov(int32 InPlayerIndex = 0) const;

	UFUNCTION(BlueprintPure)
	float GetCurrentCameraFov(bool bRefresh = false, int32 InPlayerIndex = 0) const;

	//////////////////////////////////////////////////////////////////////////
	/// Network
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	bool EDC_DrawCameraRange() const { return CameraMoveRange.IsValid == 1; };
};
