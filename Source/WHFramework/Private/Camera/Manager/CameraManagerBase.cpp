// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/Manager/CameraManagerBase.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraModuleStatics.h"
#include "Camera/Actor/CameraActorBase.h"
#include "Common/CommonStatics.h"

ACameraManagerBase::ACameraManagerBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void ACameraManagerBase::SetViewTarget(AActor* NewViewTarget, FViewTargetTransitionParams TransitionParams)
{
	if(GetOwningPlayerController() == UCommonStatics::GetPlayerController() && UCameraModuleStatics::GetCurrentCamera() && (!NewViewTarget || !NewViewTarget->IsA<ACameraActor>()))
	{
		NewViewTarget = UCameraModuleStatics::GetCurrentCamera();
	}
	Super::SetViewTarget(NewViewTarget, TransitionParams);
}

void ACameraManagerBase::UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime)
{
	Super::UpdateViewTarget(OutVT, DeltaTime);
}
