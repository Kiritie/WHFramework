// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "SaveGame/SaveGameModuleTypes.h"

#include "CameraModuleTypes.generated.h"

UENUM(BlueprintType)
enum class ECameraCollisionMode : uint8
{
	None,
	All,
	PhysicsOnly,
	SightOnly
};

UENUM(BlueprintType)
enum class ETrackTargetMode : uint8
{
	LocationOnly,
	LocationAndRotation,
	LocationAndRotationAndDistance,
	LocationAndRotationAndDistanceOnce
};

UENUM(BlueprintType)
enum class ETrackTargetSpace : uint8
{
	Local,
	World
};

USTRUCT(BlueprintType)
struct FCameraParams
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
