// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"


#include "MediaModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "MediaModuleBPLibrary.generated.h"

class AMediaModule;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UMediaModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "NetworkModuleBPLibrary")
	static void AddMediaPlayerToList(class AMediaPlayerBase* InMediaPlayer);

	UFUNCTION(BlueprintCallable, Category = "NetworkModuleBPLibrary")
	static void RemoveMediaPlayerFromList(class AMediaPlayerBase* InMediaPlayer);

	UFUNCTION(BlueprintCallable, Category = "NetworkModuleBPLibrary")
	static void RemoveMediaPlayerFromListByName(const FName InMediaPlayerName);

	UFUNCTION(BlueprintPure, Category = "NetworkModuleBPLibrary")
	static class AMediaPlayerBase* GetMediaPlayerByName(const FName InMediaPlayerName);
	
	UFUNCTION(BlueprintCallable, Category = "NetworkModuleBPLibrary")
	static void PlayMediaPlayerMovie(const FName InMediaPlayerName, const FName InMovieName, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "NetworkModuleBPLibrary")
	static void PlayMediaPlayerMovieWithDelegate(const FName InMediaPlayerName, const FName InMovieName, const FOnMoviePlayFinishedSingleDelegate& InOnMoviePlayFinishedDelegate, bool bMulticast = false);
	
	UFUNCTION(BlueprintCallable, Category = "NetworkModuleBPLibrary")
	static void StopMediaPlayerMovie(const FName InMediaPlayerName, bool bSkip, bool bMulticast = false);
};
