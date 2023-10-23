// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Camera/PlayerCameraManager.h"

#include "CameraManagerBase.generated.h"

/**
 *
 */
UCLASS(notplaceable, MinimalAPI)
class ACameraManagerBase : public APlayerCameraManager
{
	GENERATED_BODY()

public:
	ACameraManagerBase(const FObjectInitializer& ObjectInitializer);

public:
	virtual void SetViewTarget(AActor* NewViewTarget, FViewTargetTransitionParams TransitionParams) override;

protected:
	virtual void UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime) override;
};
