// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Camera/Actor/CameraActorBase.h"
#include "CCTVCameraActor.generated.h"

class ACameraPointBase;

UENUM(BlueprintType)
enum class ECCTVCameraShotMode : uint8
{
	None,
	Fixed,
	Zoom,
	Rotate,
	ZoomAndRotate
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCCTVCameraShotData
{
	GENERATED_BODY()

public:
	FCCTVCameraShotData()
	{
		CameraShotMode = ECCTVCameraShotMode::None;
		CameraZoomSpeed = 0.f;
		CameraMinDistance = 0.f;
		CameraMaxDistance = 0.f;
		CameraRotateSpeed = 0.f;
		CameraViewData = FCameraViewData();
	}

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ECCTVCameraShotMode CameraShotMode;
	
	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "CameraShotMode == ECCTVCameraShotMode::Zoom || CameraShotMode == ECCTVCameraShotMode::ZoomAndRotate"), BlueprintReadOnly)
	float CameraZoomSpeed;
			
	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "CameraShotMode == ECCTVCameraShotMode::Zoom || CameraShotMode == ECCTVCameraShotMode::ZoomAndRotate"), BlueprintReadOnly)
	float CameraMinDistance;
	
	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "CameraShotMode == ECCTVCameraShotMode::Zoom || CameraShotMode == ECCTVCameraShotMode::ZoomAndRotate"), BlueprintReadOnly)
	float CameraMaxDistance;

	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "CameraShotMode == ECCTVCameraShotMode::Rotate || CameraShotMode == ECCTVCameraShotMode::ZoomAndRotate"), BlueprintReadOnly)
	float CameraRotateSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FCameraViewData CameraViewData;
};

UCLASS()
class WHFRAMEWORK_API ACCTVCameraActor : public ACameraActorBase
{
	GENERATED_BODY()

public:
	ACCTVCameraActor();

public:
	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation() override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnSwitch_Implementation() override;

	virtual void OnUnSwitch_Implementation() override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float CameraShotMinInterval;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float CameraShotMaxInterval;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float CameraShotRemainTime;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float CameraShotFadeTime;
		
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float CameraShotMinPitch;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float CameraShotMaxPitch;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float CameraShotZoomSpeed;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float CameraShotRotateSpeed;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly)
	TArray<ACameraPointBase*> DefaultCameraShotPoints;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<ACameraPointBase*> CurrentCameraShotPoints;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FCCTVCameraShotData CurrentCameraShotData;
};
