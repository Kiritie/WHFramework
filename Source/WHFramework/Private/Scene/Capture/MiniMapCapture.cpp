// Fill out your copyright notice in the Description page of Project Settings.

#include "Scene/Capture/MiniMapCapture.h"

#include "Components/SceneCaptureComponent2D.h"

// Sets default values
AMiniMapCapture::AMiniMapCapture()
{
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
	
	Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
	Capture->SetupAttachment(RootComponent);
	Capture->SetRelativeLocationAndRotation(FVector(0.f, 0.f, 500.f), FRotator(-90.f, 0.f, 0.f));
	Capture->ProjectionType = ECameraProjectionMode::Orthographic;
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
	Capture->bCaptureEveryFrame = true;
	Capture->bAutoActivate = false;
}
