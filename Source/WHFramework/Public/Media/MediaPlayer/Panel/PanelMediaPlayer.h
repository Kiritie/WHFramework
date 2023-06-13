// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Components/ArrowComponent.h"
#include "Media/MediaPlayer/MediaPlayerBase.h"
#include "PanelMediaPlayer.generated.h"

/**
 * 
 */

 class UMediaPlayer;

UCLASS()
class WHFRAMEWORK_API APanelMediaPlayer : public AMediaPlayerBase
{
	GENERATED_BODY()

public:
	APanelMediaPlayer();

protected:
	virtual void BeginPlay() override;

public:
	virtual void PlayMovieImpl_Implementation(const FName InMovieName) override;

	virtual void StopMovieImpl_Implementation(bool bSkip) override;

protected:
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Components)
	UStaticMeshComponent* PanelMediaMesh;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Components)
	UArrowComponent* Arrow;

	UPROPERTY(BlueprintReadOnly, VisibleDefaultsOnly, Category = "Media")
	UMediaPlayer* PanelMediaPlayer;

	UPROPERTY()
	FName MediaName;
	
public:
	UPROPERTY(BlueprintReadOnly, EditInstanceOnly, Category = "Media")
	TMap<FName, class UMediaSource*> MediaList;

protected:
	UFUNCTION()
	virtual void PanelMediaOnEndReached();
};
