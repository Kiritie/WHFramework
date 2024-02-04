// Fill out your copyright notice in the Description page of Project Settings.


#include "Video/MediaPlayer/Panel/PanelMediaPlayer.h"

#include "MediaPlayer.h"
#include "Camera/CameraModuleStatics.h"
#include "Kismet/KismetMathLibrary.h"

APanelMediaPlayer::APanelMediaPlayer()
{
	PrimaryActorTick.bCanEverTick = true;
	
	Name = "TV";

	bOrientCamera = false;
	
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(FName("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetRelativeRotation(FRotator(0.0f,-90.0f,0.0f));
	Mesh->SetRelativeScale3D(FVector(2.0f,2.0f,2.0f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PanelMediaMeshFinder(TEXT("StaticMesh'/WHFramework/Video/Misc/Panel/SM_PanelMedia.SM_PanelMedia'"));
	if(PanelMediaMeshFinder.Succeeded())
	{
		Mesh->SetStaticMesh(PanelMediaMeshFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterial> PanelMediaMatFinder(TEXT("Material'/WHFramework/Video/Misc/Panel/M_PanelMedia.M_PanelMedia'"));
	if(PanelMediaMeshFinder.Succeeded())
	{
		Mesh->SetMaterial(1, PanelMediaMatFinder.Object);
	}

	Arrow = CreateDefaultSubobject<UArrowComponent>(FName("Arrow"));
	Arrow->SetupAttachment(RootComponent);
	Arrow->ArrowSize = 0.5f;

	static ConstructorHelpers::FObjectFinder<UMediaPlayer> PanelMediaPlayerFinder(TEXT("MediaPlayer'/WHFramework/Video/Misc/Panel/MP_PanelMedia.MP_PanelMedia'"));
	if(PanelMediaPlayerFinder.Succeeded())
	{
		MediaPlayer = PanelMediaPlayerFinder.Object;
	}
}

void APanelMediaPlayer::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void APanelMediaPlayer::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);

	if(bOrientCamera)
	{
		const FRotator Rotator = UKismetMathLibrary::FindLookAtRotation(GetActorLocation(), UCameraModuleStatics::GetCameraLocation(true));
		SetActorRotation(FRotator(0.f, Rotator.Yaw, 0.f));
	}
}
