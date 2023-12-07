// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Capture/VoxelCapture.h"

#include "Components/SceneCaptureComponent2D.h"

// Sets default values
AVoxelCapture::AVoxelCapture()
{
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif

	Capture2D = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("VoxelsCapture"));
	Capture2D->SetupAttachment(RootComponent);
	Capture2D->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -1000.f), FRotator(90.f, 0.f, 0.f));
	static ConstructorHelpers::FObjectFinder<UTextureRenderTarget2D> VoxelsPreviewTexFinder(TEXT("TextureRenderTarget2D'/WHFramework/Voxel/Textures/RT_VoxelPreview.RT_VoxelPreview'"));
	if(VoxelsPreviewTexFinder.Succeeded())
	{
		Capture2D->TextureTarget = VoxelsPreviewTexFinder.Object;
	}
	Capture2D->ProjectionType = ECameraProjectionMode::Orthographic;
	Capture2D->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Capture2D->bCaptureEveryFrame = true;
}
