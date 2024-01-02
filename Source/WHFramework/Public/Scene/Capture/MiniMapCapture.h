// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Common/Base/WHActor.h"
#include "MiniMapCapture.generated.h"

/**
 * 体素模块
 */
UCLASS()
class WHFRAMEWORK_API AMiniMapCapture : public AWHActor
{
	GENERATED_BODY()

public:
	AMiniMapCapture();

public:
#if WITH_EDITOR
	virtual bool IsUserManaged() const override { return false; }

	virtual bool CanChangeIsSpatiallyLoadedFlag() const override { return false; }
#endif

	//////////////////////////////////////////////////////////////////////////
	// Components
protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	USceneCaptureComponent2D* Capture;

public:
	UFUNCTION(BlueprintPure)
	USceneCaptureComponent2D* GetCapture() const { return Capture;}
};
