// Fill out your copyright notice in the Description page of Project Settings.

#pragma once



#include "MediaModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "MediaModuleBPLibrary.generated.h"

class AMediaModule;
class AMediaPlayerBase;
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
	static void RemoveMediaPlayerFromListByName(const FName InName);

	UFUNCTION(BlueprintPure, Category = "NetworkModuleBPLibrary")
	static AMediaPlayerBase* GetMediaPlayerByName(const FName InName);
	
	UFUNCTION(BlueprintCallable, Category = "NetworkModuleBPLibrary")
	static void PlayMediaPlayerMovie(const FName InName, const FName InMovieName, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "NetworkModuleBPLibrary")
	static void PlayMediaPlayerMovieWithDelegate(const FName InName, const FName InMovieName, const FOnMoviePlayFinishedSingleDelegate& InOnPlayFinished, bool bMulticast = false);
	
	UFUNCTION(BlueprintCallable, Category = "NetworkModuleBPLibrary")
	static void StopMediaPlayerMovie(const FName InName, bool bSkip, bool bMulticast = false);
};
