// Fill out your copyright notice in the Description page of Project Settings.


#include "Media/MediaPlayer/Panel/PanelMediaPlayer.h"

#include "MediaPlayer.h"
#include "Debug/DebugModuleTypes.h"

APanelMediaPlayer::APanelMediaPlayer()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(FName("DefaultScene"));

	Name = "TV";
	
	PanelMediaMesh = CreateDefaultSubobject<UStaticMeshComponent>(FName("PanelMediaMesh"));
	if (PanelMediaMesh)
	{
		PanelMediaMesh->SetupAttachment(RootComponent);
		PanelMediaMesh->SetRelativeRotation(FRotator(0.0f,-90.0f,0.0f));
		PanelMediaMesh->SetRelativeScale3D(FVector(2.0f,2.0f,2.0f));
		
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("StaticMesh'/WHFramework/Media/Panel/SM_PanelMedia.SM_PanelMedia'"));
		if(Mesh)
		{
			PanelMediaMesh->SetStaticMesh(Mesh);
		}

		UMaterial* Mat_PanelMaterial = LoadObject<UMaterial>(nullptr, TEXT("Material'/WHFramework/Media/Panel/M_PanelMedia.M_PanelMedia'"));
		if (Mat_PanelMaterial)
		{
			PanelMediaMesh->SetMaterial(1,Mat_PanelMaterial);
		}
	}

	Arrow = CreateDefaultSubobject<UArrowComponent>(FName("Arrow"));
	if (Arrow)
	{
		Arrow->ArrowSize = 0.5f;
	}

	PanelMediaPlayer = LoadObject<UMediaPlayer>(nullptr, TEXT("MediaPlayer'/WHFramework/Media/Panel/MP_PanelMedia.MP_PanelMedia'"));
}

void APanelMediaPlayer::BeginPlay()
{
	Super::BeginPlay();

	PanelMediaPlayer->OnEndReached.AddDynamic(this, &APanelMediaPlayer::PanelMediaOnEndReached);
}

void APanelMediaPlayer::PlayMovieImpl_Implementation(const FName InMovieName)
{
	Super::PlayMovieImpl_Implementation(InMovieName);
	
	if (PanelMediaPlayer && MediaList.Contains(InMovieName))
	{
		MediaName = InMovieName;
		PanelMediaPlayer->OpenSource(MediaList[InMovieName]);
		OnMoviePlayStartingDelegate.Broadcast(InMovieName);
	}
}

void APanelMediaPlayer::StopMovieImpl_Implementation(bool bSkip)
{
	Super::StopMovieImpl_Implementation(bSkip);

	if(PanelMediaPlayer)
	{
		PanelMediaPlayer->Close();
	}
}

void APanelMediaPlayer::PanelMediaOnEndReached()
{
	OnMoviePlayFinishedDelegate.Broadcast(MediaName);

	if (OnMoviePlayFinishedSingleDelegate.IsBound())
	{
		auto& TempDelegate = OnMoviePlayFinishedSingleDelegate;
		OnMoviePlayFinishedSingleDelegate.Clear();
		TempDelegate.Execute(MediaName);
	}
}
