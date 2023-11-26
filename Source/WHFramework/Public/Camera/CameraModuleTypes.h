// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "Math/MathTypes.h"
#include "Parameter/ParameterModuleTypes.h"
#include "SaveGame/SaveGameModuleTypes.h"

#include "CameraModuleTypes.generated.h"

class ACameraActorBase;

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
		InParams.ParseIntoArray(ParamsArr, TEXT("|"));
		CameraLocation.InitFromString(ParamsArr[0]);
		CameraRotation.InitFromString(ParamsArr[1]);
		CameraDistance = FCString::Atof(*ParamsArr[2]);
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
		CameraViewOffset = FVector::ZeroVector;
		CameraViewYaw = 0.f;
		CameraViewPitch = 0.f;
		CameraViewDistance = 0.f;
	}

public:
	void GetCameraParams();

	void SetCameraParams(const FCameraParams& InCameraParams);

public:
	UPROPERTY(BlueprintReadOnly)
	AActor* CameraViewTarget;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<ACameraActorBase> CameraViewActor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ECameraViewMode CameraViewMode;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ECameraViewSpace CameraViewSpace;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditConditionHides, EditCondition = "CameraViewMode == ECameraViewMode::Duration"))
	EEaseType CameraViewEaseType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditConditionHides, EditCondition = "CameraViewMode == ECameraViewMode::Duration"))
	float CameraViewDuration;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector CameraViewOffset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float CameraViewYaw;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float CameraViewPitch;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float CameraViewDistance;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCameraViewData : public FParamData
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
	virtual void FromParams(const TArray<FParameter>& InParams) override;

	virtual TArray<FParameter> ToParams() const override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<AActor> CameraViewTarget;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bTrackTarget;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditConditionHides, EditCondition = "bTrackTarget == true"))
	ECameraTrackMode TrackTargetMode;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FCameraViewParams CameraViewParams;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCameraModuleSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FCameraModuleSaveData()
	{
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
	// Move
	UPROPERTY(EditAnywhere, Category = "CameraControl|Move")
	bool bReverseCameraPanMove;
	
	UPROPERTY(EditAnywhere, Category = "CameraControl|Move")
	float CameraMoveRate;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Move")
	bool bSmoothCameraMove;

	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "bSmoothCameraMove == true"), Category = "CameraControl|Move")
	float CameraMoveSpeed;

	// Rotate
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

	// Zoom
	UPROPERTY(EditAnywhere, Category = "CameraControl|Zoom")
	float CameraZoomRate;

	UPROPERTY(EditAnywhere, Category = "CameraControl|Zoom")
	bool bSmoothCameraZoom;

	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "bSmoothCameraZoom == true"), Category = "CameraControl|Zoom")
	float CameraZoomSpeed;

public:
	virtual void MakeSaved() override
	{
		Super::MakeSaved();
	}
};
