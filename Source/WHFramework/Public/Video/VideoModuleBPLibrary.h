// Fill out your copyright notice in the Description page of Project Settings.

#pragma once



#include "VideoModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "VideoModuleBPLibrary.generated.h"

class AVideoModule;
class AMediaPlayerBase;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UVideoModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "VideoModuleBPLibrary")
	static void AddMediaPlayerToList(class AMediaPlayerBase* InMediaPlayer);

	UFUNCTION(BlueprintCallable, Category = "VideoModuleBPLibrary")
	static void RemoveMediaPlayerFromList(class AMediaPlayerBase* InMediaPlayer);

	UFUNCTION(BlueprintCallable, Category = "VideoModuleBPLibrary")
	static void RemoveMediaPlayerFromListByName(const FName InName);

	UFUNCTION(BlueprintPure, Category = "VideoModuleBPLibrary")
	static AMediaPlayerBase* GetMediaPlayerByName(const FName InName);
	
	UFUNCTION(BlueprintCallable, Category = "VideoModuleBPLibrary")
	static void PlayMediaPlayerMovie(const FName InName, const FName InMovieName, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "VideoModuleBPLibrary")
	static void PlayMediaPlayerMovieWithDelegate(const FName InName, const FName InMovieName, const FOnMoviePlayFinishedSingleDelegate& InOnPlayFinished, bool bMulticast = false);
	
	UFUNCTION(BlueprintCallable, Category = "VideoModuleBPLibrary")
	static void StopMediaPlayerMovie(const FName InName, bool bSkip, bool bMulticast = false);
};
