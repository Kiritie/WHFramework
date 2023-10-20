// Fill out your copyright notice in the Description page of Project Settings.


#include "Video/MediaPlayer/Panel/PanelMediaPlayer.h"

#include "MediaPlayer.h"
#include "Video/VideoModuleTypes.h"

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

		static ConstructorHelpers::FObjectFinder<UStaticMesh> PanelMediaMeshFinder(TEXT("StaticMesh'/WHFramework/Media/Panel/SM_PanelMedia.SM_PanelMedia'"));
		if(PanelMediaMeshFinder.Succeeded())
		{
			PanelMediaMesh->SetStaticMesh(PanelMediaMeshFinder.Object);
		}

		static ConstructorHelpers::FObjectFinder<UMaterial> PanelMediaMatFinder(TEXT("Material'/WHFramework/Media/Panel/M_PanelMedia.M_PanelMedia'"));
		if(PanelMediaMeshFinder.Succeeded())
		{
			PanelMediaMesh->SetMaterial(1, PanelMediaMatFinder.Object);
		}
	}

	Arrow = CreateDefaultSubobject<UArrowComponent>(FName("Arrow"));
	if (Arrow)
	{
		Arrow->ArrowSize = 0.5f;
	}

	static ConstructorHelpers::FObjectFinder<UMediaPlayer> PanelMediaPlayerFinder(TEXT("MediaPlayer'/WHFramework/Media/Panel/MP_PanelMedia.MP_PanelMedia'"));
	if(PanelMediaPlayerFinder.Succeeded())
	{
		PanelMediaPlayer = PanelMediaPlayerFinder.Object;
	}
}

void APanelMediaPlayer::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();

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
