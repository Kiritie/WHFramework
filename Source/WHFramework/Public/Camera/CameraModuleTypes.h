// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "Math/MathTypes.h"
#include "Parameter/ParameterModuleTypes.h"
#include "SaveGame/SaveGameModuleTypes.h"

#include "CameraModuleTypes.generated.h"

class ACameraActorBase;
class ACameraPointBase;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCameraSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bEnablePanZMove = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bReversePanMove = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MoveRate = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bSmoothMove = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MoveSpeed = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bReversePitch = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TurnRate = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float LookUpRate = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bSmoothRotate = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float RotateSpeed = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ZoomRate = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bSmoothZoom = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ZoomSpeed = 5.f;
};

UENUM(BlueprintType)
enum class ECameraCollisionMode : uint8
{
	None,
	All,
	PhysicsOnly,
	SightOnly
};

UENUM(BlueprintType)
enum class ECameraTrackMode : uint8
{
	LocationOnly,
	LocationAndRotation,
	LocationAndRotationOnce,
	LocationAndRotationAndDistance,
	LocationAndRotationAndDistanceOnce,
	LocationAndRotationOnceAndDistanceOnce
};

UENUM(BlueprintType)
enum class ECameraSmoothMode : uint8
{
	None = 0,
	LocationOnly = 1 << 0,
	RotationOnly = 1 << 1,
	DistanceOnly = 1 << 2,
	LocationAndRotation = LocationOnly | RotationOnly,
	RotationAndDistance = RotationOnly | DistanceOnly,
	LocationAndDistance = LocationOnly | DistanceOnly,
	All = LocationOnly | RotationOnly | DistanceOnly
};

UENUM(BlueprintType)
enum class ECameraControlMode : uint8
{
	None = 0,
	LocationOnly = 1 << 0,
	RotationOnly = 1 << 1,
	DistanceOnly = 1 << 2,
	LocationAndRotation = LocationOnly | RotationOnly,
	RotationAndDistance = RotationOnly | DistanceOnly,
	LocationAndDistance = LocationOnly | DistanceOnly,
	All = LocationOnly | RotationOnly | DistanceOnly
};

UENUM(BlueprintType)
enum class ECameraViewMode : uint8
{
	None,
	Instant,
	Smooth,
	Duration
};

UENUM(BlueprintType)
enum class ECameraViewSpace : uint8
{
	Local,
	World
};

UENUM(BlueprintType)
enum class ECameraResetMode : uint8
{
	DefaultPoint,
	CurrentPoint,
	CachedData
};

UENUM(BlueprintType, meta = (Bitflags))
enum class ECameraViewProperty : uint8
{
	None = 0,
	Location = 1 << 0,
	Offset = 1 << 1,
	Rotation = 1 << 2,
	Distance = 1 << 3,
	FOV = 1 << 4
};

ENUM_CLASS_FLAGS(ECameraViewProperty)

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCameraTrackProfile
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Offset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Distance = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MinPitch = -90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxPitch = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ECameraTrackMode TrackMode = ECameraTrackMode::LocationAndRotationAndDistanceOnce;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ECameraSmoothMode SmoothMode = ECameraSmoothMode::All;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ECameraControlMode ControlMode = ECameraControlMode::All;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCameraTargetRequest
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<AActor> Target = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FCameraTrackProfile Profile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ECameraViewMode ViewMode = ECameraViewMode::Smooth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ECameraViewSpace ViewSpace = ECameraViewSpace::Local;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bResetRotation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bInstant = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bAllowControl = true;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCameraTransitionParams
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ECameraViewMode Mode = ECameraViewMode::Smooth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Duration = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EEaseType EaseType = EEaseType::Linear;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bForce = true;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCameraViewRequest
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Bitmask, BitmaskEnum = "/Script/WHFramework.ECameraViewProperty"))
	int32 Properties = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Offset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Distance = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FOV = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FCameraTransitionParams Transition;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCameraRuntimeState
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<ACameraActorBase> Camera = nullptr;

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<AActor> Target = nullptr;

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<ACameraPointBase> CameraPoint = nullptr;

	UPROPERTY(BlueprintReadOnly)
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	FVector Offset = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly)
	float Distance = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float FOV = 90.f;

	UPROPERTY(BlueprintReadOnly)
	bool bTracking = false;

	UPROPERTY(BlueprintReadOnly)
	bool bTransitioning = false;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCameraParams
{
	GENERATED_BODY()

public:
	FCameraParams()
	{
		CameraLocation = FVector::ZeroVector;
		CameraRotation = FRotator::ZeroRotator;
		CameraDistance = 0.f;
	}

	FCameraParams(const FString& InParams)
	{
		TArray<FString> ParamsArr;
		if(InParams.ParseIntoArray(ParamsArr, TEXT("|")) == 3)
		{
			CameraLocation.InitFromString(ParamsArr[0]);
			CameraRotation.InitFromString(ParamsArr[1]);
			CameraDistance = FCString::Atof(*ParamsArr[2]);
		}
	}

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FVector CameraLocation;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FRotator CameraRotation;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	float CameraDistance;

public:
	FString ToString() const
	{
		return FString::Printf(TEXT("%s|%s|%f"), *CameraLocation.ToString(), *CameraRotation.ToString(), CameraDistance);
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCameraViewParams
{
	GENERATED_BODY()

public:
	FCameraViewParams()
	{
		CameraViewTarget = nullptr;
		CameraViewActor = nullptr;
		CameraViewMode = ECameraViewMode::None;
		CameraViewSpace = ECameraViewSpace::Local;
		CameraViewEaseType = EEaseType::Linear;
		CameraViewDuration = 1.f;
		CameraViewLocation = FVector::ZeroVector;
		CameraViewOffset = FVector::ZeroVector;
		CameraViewYaw = 0.f;
		CameraViewPitch = 0.f;
		CameraViewDistance = 0.f;
		CameraViewFov = -1.f;
	}

public:
	bool IsValid() const
	{
		return CameraViewMode != ECameraViewMode::None;
	}

	void GetCameraParams(AActor* InCameraViewTarget = nullptr);

	void SetCameraParams(const FCameraParams& InCameraParams, AActor* InCameraViewTarget = nullptr);

	FVector GetCameraLocation(bool bApplyViewSpace = false) const;
	
	FVector GetCameraOffset() const;
	
	float GetCameraYaw() const;
	
	float GetCameraPitch() const;
	
	float GetCameraDistance() const;
	
	float GetCameraFov() const;

public:
	UPROPERTY(BlueprintReadOnly, Transient)
	AActor* CameraViewTarget;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<ACameraActorBase> CameraViewActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ECameraViewMode CameraViewMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ECameraViewSpace CameraViewSpace;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditConditionHides, EditCondition = "CameraViewMode == ECameraViewMode::Duration"))
	EEaseType CameraViewEaseType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditConditionHides, EditCondition = "CameraViewMode == ECameraViewMode::Duration"))
	float CameraViewDuration;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector CameraViewLocation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector CameraViewOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CameraViewYaw;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CameraViewPitch;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CameraViewDistance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CameraViewFov;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCameraViewData
{
	GENERATED_BODY()

public:
	FCameraViewData()
	{
		CameraViewTarget = nullptr;
		bTrackTarget = false;
		TrackTargetMode = ECameraTrackMode::LocationOnly;
		CameraViewParams = FCameraViewParams();
	}

	FCameraViewData(AActor* InCameraViewTarget, bool bInTrackTarget, ECameraTrackMode InTrackTargetMode, const FCameraViewParams& InCameraViewParams = FCameraViewParams())
	{
		CameraViewTarget = InCameraViewTarget;
		bTrackTarget = bInTrackTarget;
		TrackTargetMode = InTrackTargetMode;
		CameraViewParams = InCameraViewParams;
		CameraViewParams.CameraViewTarget = InCameraViewTarget;
	}

public:
	void FromParams(const TArray<FParameter>& InParams);

	TArray<FParameter> ToParams() const;

public:
	bool IsValid() const
	{
		return CameraViewParams.IsValid();
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<AActor> CameraViewTarget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bTrackTarget;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditConditionHides, EditCondition = "bTrackTarget == true"))
	ECameraTrackMode TrackTargetMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FCameraViewParams CameraViewParams;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCameraModuleSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FCameraModuleSaveData()
	{
		bEnableCameraPanZMove = true;
		bReverseCameraPanMove = false;
		CameraMoveRate = 300.f;
		bSmoothCameraMove = true;
		CameraMoveSpeed = 5.f;

		bReverseCameraPitch = false;
		CameraTurnRate = 90.f;
		CameraLookUpRate = 90.f;
		bSmoothCameraRotate = true;
		CameraRotateSpeed = 5.f;

		CameraZoomRate = 150.f;
		bSmoothCameraZoom = true;
		CameraZoomSpeed = 5.f;
	}

public:
	FCameraSettings ToSettings() const
	{
		FCameraSettings Result;

		Result.bEnablePanZMove = bEnableCameraPanZMove;
		Result.bReversePanMove = bReverseCameraPanMove;
		Result.MoveRate = CameraMoveRate;
		Result.bSmoothMove = bSmoothCameraMove;
		Result.MoveSpeed = CameraMoveSpeed;
		Result.bReversePitch = bReverseCameraPitch;
		Result.TurnRate = CameraTurnRate;
		Result.LookUpRate = CameraLookUpRate;
		Result.bSmoothRotate = bSmoothCameraRotate;
		Result.RotateSpeed = CameraRotateSpeed;
		Result.ZoomRate = CameraZoomRate;
		Result.bSmoothZoom = bSmoothCameraZoom;
		Result.ZoomSpeed = CameraZoomSpeed;

		return Result;
	}

	void FromSettings(const FCameraSettings& InSettings)
	{
		bEnableCameraPanZMove = InSettings.bEnablePanZMove;
		bReverseCameraPanMove = InSettings.bReversePanMove;
		CameraMoveRate = InSettings.MoveRate;
		bSmoothCameraMove = InSettings.bSmoothMove;
		CameraMoveSpeed = InSettings.MoveSpeed;
		bReverseCameraPitch = InSettings.bReversePitch;
		CameraTurnRate = InSettings.TurnRate;
		CameraLookUpRate = InSettings.LookUpRate;
		bSmoothCameraRotate = InSettings.bSmoothRotate;
		CameraRotateSpeed = InSettings.RotateSpeed;
		CameraZoomRate = InSettings.ZoomRate;
		bSmoothCameraZoom = InSettings.bSmoothZoom;
		CameraZoomSpeed = InSettings.ZoomSpeed;
	}

	// Move
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CameraControl|Move", meta = (SettingPage = "Camera", SettingCategory = "Move", SettingOrder = "0"))
	bool bEnableCameraPanZMove;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CameraControl|Move", meta = (SettingPage = "Camera", SettingCategory = "Move", SettingOrder = "1"))
	bool bReverseCameraPanMove;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CameraControl|Move", meta = (SettingPage = "Camera", SettingCategory = "Move", SettingOrder = "2", UIMin = "0.0", UIMax = "1000.0", Delta = "1.0", SettingDecimalPlaces = "0"))
	float CameraMoveRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CameraControl|Move", meta = (SettingPage = "Camera", SettingCategory = "Move", SettingOrder = "3"))
	bool bSmoothCameraMove;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CameraControl|Move", meta = (EditConditionHides, EditCondition = "bSmoothCameraMove == true", SettingPage = "Camera", SettingCategory = "Move", SettingOrder = "4", UIMin = "0.0", UIMax = "20.0", Delta = "0.1", SettingDecimalPlaces = "1"))
	float CameraMoveSpeed;

	// Rotate
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CameraControl|Rotate", meta = (SettingPage = "Camera", SettingCategory = "Rotate", SettingOrder = "0"))
	bool bReverseCameraPitch;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CameraControl|Rotate", meta = (SettingPage = "Camera", SettingCategory = "Rotate", SettingOrder = "1", UIMin = "0.0", UIMax = "300.0", Delta = "1.0", SettingDecimalPlaces = "0"))
	float CameraTurnRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CameraControl|Rotate", meta = (SettingPage = "Camera", SettingCategory = "Rotate", SettingOrder = "2", UIMin = "0.0", UIMax = "300.0", Delta = "1.0", SettingDecimalPlaces = "0"))
	float CameraLookUpRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CameraControl|Rotate", meta = (SettingPage = "Camera", SettingCategory = "Rotate", SettingOrder = "3"))
	bool bSmoothCameraRotate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CameraControl|Rotate", meta = (EditConditionHides, EditCondition = "bSmoothCameraRotate == true", SettingPage = "Camera", SettingCategory = "Rotate", SettingOrder = "4", UIMin = "0.0", UIMax = "20.0", Delta = "0.1", SettingDecimalPlaces = "1"))
	float CameraRotateSpeed;

	// Zoom
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CameraControl|Zoom", meta = (SettingPage = "Camera", SettingCategory = "Zoom", SettingOrder = "0", UIMin = "0.0", UIMax = "1000.0", Delta = "1.0", SettingDecimalPlaces = "0"))
	float CameraZoomRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CameraControl|Zoom", meta = (SettingPage = "Camera", SettingCategory = "Zoom", SettingOrder = "1"))
	bool bSmoothCameraZoom;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CameraControl|Zoom", meta = (EditConditionHides, EditCondition = "bSmoothCameraZoom == true", SettingPage = "Camera", SettingCategory = "Zoom", SettingOrder = "2", UIMin = "0.0", UIMax = "20.0", Delta = "0.1", SettingDecimalPlaces = "1"))
	float CameraZoomSpeed;

public:
	virtual void MakeSaved() override
	{
		Super::MakeSaved();
	}
};
