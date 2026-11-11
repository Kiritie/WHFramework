// Fill out your copyright notice in the Description page of Project Settings.

#pragma once



#include "VideoModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "VideoModuleStatics.generated.h"

class UVideoModule;
class AMediaPlayerBase;
class APlayerController;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UVideoModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "VideoModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void AddMediaPlayerToList(const UObject* WorldContextObject, AMediaPlayerBase* InMediaPlayer);

	UFUNCTION(BlueprintCallable, Category = "VideoModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void RemoveMediaPlayerFromList(const UObject* WorldContextObject, AMediaPlayerBase* InMediaPlayer);

	UFUNCTION(BlueprintCallable, Category = "VideoModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void RemoveMediaPlayerFromListByName(const UObject* WorldContextObject, const FName InName);

	UFUNCTION(BlueprintPure, Category = "VideoModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static AMediaPlayerBase* GetMediaPlayerByName(const UObject* WorldContextObject, const FName InName);

public:
	UFUNCTION(BlueprintCallable, Category = "VideoModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void PlayMovieForMediaPlayer(const UObject* WorldContextObject, const FName InName, const FName InMovieName, bool bMulticast = false, APlayerController* InController = nullptr);

	UFUNCTION(BlueprintCallable, Category = "VideoModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void PlayMovieForMediaPlayerWithDelegate(const UObject* WorldContextObject, const FName InName, const FName InMovieName, const FOnMoviePlayFinishedSingleDelegate& InOnPlayFinished, bool bMulticast = false, APlayerController* InController = nullptr);
		
	UFUNCTION(BlueprintCallable, Category = "VideoModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void PauseMovieForMediaPlayer(const UObject* WorldContextObject, const FName InName, bool bMulticast = false, APlayerController* InController = nullptr);
	
	UFUNCTION(BlueprintCallable, Category = "VideoModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void SeekMovieForMediaPlayer(const UObject* WorldContextObject, const FName InName, const FTimespan& InTimespan, bool bMulticast = false, APlayerController* InController = nullptr);

	UFUNCTION(BlueprintCallable, Category = "VideoModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void StopMovieForMediaPlayer(const UObject* WorldContextObject, const FName InName, bool bSkip, bool bMulticast = false, APlayerController* InController = nullptr);
};
