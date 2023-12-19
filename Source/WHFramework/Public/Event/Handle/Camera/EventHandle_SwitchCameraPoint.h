// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Event/Handle/InstancedEventHandleBase.h"

#include "EventHandle_SwitchCameraPoint.generated.h"

class ACameraPointBase;
/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UEventHandle_SwitchCameraPoint : public UInstancedEventHandleBase
{
	GENERATED_BODY()

public:
	UEventHandle_SwitchCameraPoint();
	
public:
	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	virtual void Parse_Implementation(const TArray<FParameter>& InParams) override;

	virtual TArray<FParameter> Pack_Implementation() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ACameraPointBase* CameraPoint;
};
