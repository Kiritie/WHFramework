// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Step/Base/StepBase.h"
#include "Step_PlayMediaPlayerMovie.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UStep_PlayMediaPlayerMovie : public UStepBase
{
	GENERATED_BODY()

public:
	UStep_PlayMediaPlayerMovie();

	//////////////////////////////////////////////////////////////////////////
	/// Step
public:
	virtual void OnInitialize() override;
	
	virtual void OnEnter(UStepBase* InLastStep) override;

	virtual void OnLeave() override;

protected:
	UFUNCTION()
	void ServerOnMoviePlayFinished(const FName& InMovieName);

	//////////////////////////////////////////////////////////////////////////
	/// Character
public:
	/// 角色名称
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Media")
	FName PlayerName;
	
	/// 视频播放名称
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Media")
	FName MovieName;
};
