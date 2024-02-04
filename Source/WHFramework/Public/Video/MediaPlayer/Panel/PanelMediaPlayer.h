// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Components/ArrowComponent.h"
#include "Video/MediaPlayer/MediaPlayerBase.h"
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

public:
	virtual void OnInitialize_Implementation() override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* Mesh;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UArrowComponent* Arrow;

protected:
	UPROPERTY(EditAnywhere, Category = "MediaPlayerStats")
	bool bOrientCamera;
};
