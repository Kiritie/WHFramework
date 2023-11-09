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
	static void AddMediaPlayerToList(class AMediaPlayerBase* InMediaPlayer);

	UFUNCTION(BlueprintCallable, Category = "VideoModuleStatics")
	static void RemoveMediaPlayerFromList(class AMediaPlayerBase* InMediaPlayer);

	UFUNCTION(BlueprintCallable, Category = "VideoModuleStatics")
	static void RemoveMediaPlayerFromListByName(const FName InName);

	UFUNCTION(BlueprintPure, Category = "VideoModuleStatics")
	static AMediaPlayerBase* GetMediaPlayerByName(const FName InName);
	
	UFUNCTION(BlueprintCallable, Category = "VideoModuleStatics")
	static void PlayMediaPlayerMovie(const FName InName, const FName InMovieName, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "VideoModuleStatics")
	static void PlayMediaPlayerMovieWithDelegate(const FName InName, const FName InMovieName, const FOnMoviePlayFinishedSingleDelegate& InOnPlayFinished, bool bMulticast = false);
	
	UFUNCTION(BlueprintCallable, Category = "VideoModuleStatics")
	static void StopMediaPlayerMovie(const FName InName, bool bSkip, bool bMulticast = false);
};
