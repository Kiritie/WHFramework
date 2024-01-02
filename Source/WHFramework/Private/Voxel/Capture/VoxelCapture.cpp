// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Capture/VoxelCapture.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"

// Sets default values
AVoxelCapture::AVoxelCapture()
{
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
	
	Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
	Capture->SetupAttachment(RootComponent);
	Capture->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -1000.f), FRotator(90.f, 0.f, 0.f));
	static ConstructorHelpers::FObjectFinder<UTextureRenderTarget2D> VoxelsPreviewTexFinder(TEXT("TextureRenderTarget2D'/WHFramework/Voxel/Textures/RT_VoxelPreview.RT_VoxelPreview'"));
	if(VoxelsPreviewTexFinder.Succeeded())
	{
		Capture->TextureTarget = VoxelsPreviewTexFinder.Object;
	}
	Capture->ProjectionType = ECameraProjectionMode::Orthographic;
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Capture->bCaptureEveryFrame = true;
	Capture->bAutoActivate = false;
}
