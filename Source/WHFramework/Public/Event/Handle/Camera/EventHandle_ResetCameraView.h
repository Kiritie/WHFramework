// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Camera/CameraModuleTypes.h"
#include "Event/Handle/InstancedEventHandleBase.h"

#include "EventHandle_ResetCameraView.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UEventHandle_ResetCameraView : public UInstancedEventHandleBase
{
	GENERATED_BODY()

public:
	UEventHandle_ResetCameraView();
	
public:
	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	virtual void Parse_Implementation(const TArray<FParameter>& InParams) override;

	virtual TArray<FParameter> Pack_Implementation() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ECameraResetMode CameraResetMode;
};
