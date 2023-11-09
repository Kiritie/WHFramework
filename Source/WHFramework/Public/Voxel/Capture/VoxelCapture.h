// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Common/Base/WHActor.h"
#include "VoxelCapture.generated.h"

/**
 * 体素模块
 */
UCLASS()
class WHFRAMEWORK_API AVoxelCapture : public AWHActor
{
	GENERATED_BODY()

public:
	AVoxelCapture();

	//////////////////////////////////////////////////////////////////////////
	// Components
protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	USceneCaptureComponent2D* Capture2D;

public:
	USceneCaptureComponent2D* GetCapture2D() const { return Capture2D;}
};
