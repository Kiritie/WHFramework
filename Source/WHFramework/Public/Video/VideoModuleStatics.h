// Fill out your copyright notice in the Description page of Project Settings.

#pragma once



#include "VideoModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "VideoModuleStatics.generated.h"

class UVideoModule;
class AMediaPlayerBase;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UVideoModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "VideoModuleStatics")
	static void AddMediaPlayerToList(AMediaPlayerBase* InMediaPlayer);

	UFUNCTION(BlueprintCallable, Category = "VideoModuleStatics")
	static void RemoveMediaPlayerFromList(AMediaPlayerBase* InMediaPlayer);

	UFUNCTION(BlueprintCallable, Category = "VideoModuleStatics")
	static void RemoveMediaPlayerFromListByName(const FName InName);

	UFUNCTION(BlueprintPure, Category = "VideoModuleStatics")
	static AMediaPlayerBase* GetMediaPlayerByName(const FName InName);

public:
	UFUNCTION(BlueprintCallable, Category = "VideoModuleStatics")
	static void PlayMovieForMediaPlayer(const FName InName, const FName InMovieName, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "VideoModuleStatics")
	static void PlayMovieForMediaPlayerWithDelegate(const FName InName, const FName InMovieName, const FOnMoviePlayFinishedSingleDelegate& InOnPlayFinished, bool bMulticast = false);
		
	UFUNCTION(BlueprintCallable, Category = "VideoModuleStatics")
	static void PauseMovieForMediaPlayer(const FName InName, bool bMulticast = false);
	
	UFUNCTION(BlueprintCallable, Category = "VideoModuleStatics")
	static void SeekMovieForMediaPlayer(const FName InName, const FTimespan& InTimespan, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "VideoModuleStatics")
	static void StopMovieForMediaPlayer(const FName InName, bool bSkip, bool bMulticast = false);
};
