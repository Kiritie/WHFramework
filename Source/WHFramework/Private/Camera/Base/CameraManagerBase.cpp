// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/Base/CameraManagerBase.h"

#include "Camera/CameraModuleStatics.h"
#include "Camera/Base/CameraActorBase.h"

ACameraManagerBase::ACameraManagerBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void ACameraManagerBase::SetViewTarget(AActor* NewViewTarget, FViewTargetTransitionParams TransitionParams)
{
	if(UCameraModuleStatics::GetCurrentCamera())
	{
		NewViewTarget = UCameraModuleStatics::GetCurrentCamera();
	}
	Super::SetViewTarget(NewViewTarget, TransitionParams);
}

void ACameraManagerBase::UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime)
{
	Super::UpdateViewTarget(OutVT, DeltaTime);
}
