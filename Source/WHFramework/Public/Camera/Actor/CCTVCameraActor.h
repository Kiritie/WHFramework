// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Camera/Actor/CameraActorBase.h"
#include "CCTVCameraActor.generated.h"

class ACameraPointBase;

UCLASS()
class WHFRAMEWORK_API ACCTVCameraActor : public ACameraActorBase
{
	GENERATED_BODY()

public:
	ACCTVCameraActor();

public:
	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnSwitch_Implementation() override;

	virtual void OnUnSwitch_Implementation() override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float ChangePointInterval;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float ChangePointIntervalDelta;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float ChangePointViewPitch;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float ChangePointViewDistance;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float ChangePointRemainTime;
		
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float CameraRotateSpeed;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FCameraViewData CameraViewData;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<ACameraPointBase*> CameraPointQueue;
};
