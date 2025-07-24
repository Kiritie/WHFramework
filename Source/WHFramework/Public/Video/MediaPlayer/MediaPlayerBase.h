// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Common/Base/WHActor.h"

#include "Video/VideoModuleTypes.h"

#include "MediaPlayerBase.generated.h"

class UMediaSource;
class UMediaPlayer;
/**
 * 
 */
UCLASS(Blueprintable)
class WHFRAMEWORK_API AMediaPlayerBase : public AWHActor
{
	GENERATED_BODY()
	
public:
	AMediaPlayerBase();

	//////////////////////////////////////////////////////////////////////////
	/// WHActor
public:
	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation() override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnTermination_Implementation() override;

	//////////////////////////////////////////////////////////////////////////
	/// Name
protected:
	UPROPERTY(EditAnywhere, Category = "MediaPlayerStats")
	FName Name;
	
public:
	UFUNCTION(BlueprintPure)
	virtual FName GetNameP() const { return Name; }
	
	//////////////////////////////////////////////////////////////////////////
	/// Movie
protected:
	UPROPERTY(EditAnywhere, Category = "Movie")
	UMediaPlayer* MediaPlayer;
	
	UPROPERTY(EditAnywhere, Category = "Movie")
	TMap<FName, UMediaSource*> MovieList;
	
	UPROPERTY(VisibleAnywhere, Category = "Movie")
	FName MovieName;

public:
	UFUNCTION(BlueprintPure)
	UMediaPlayer* GetMediaPlayer() const { return MediaPlayer; }

	UFUNCTION(BlueprintPure)
	FName GetMovieName() const { return MovieName; }

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void PlayMovie(const FName InMovieName, bool bMulticast = false);
	
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void PlayMovieWithDelegate(const FName InMovieName, const FOnMoviePlayFinishedSingleDelegate& InOnPlayFinished, bool bMulticast = false);

	UFUNCTION(NetMulticast, Reliable)
	void MultiPlayMovie(const FName InMovieName);

protected:
	UFUNCTION(BlueprintNativeEvent)
	void PlayMovieImpl(const FName InMovieName);
	
	UFUNCTION(BlueprintNativeEvent)
	void OnMovieEndReached();
	
public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void PauseMovie(bool bMulticast = false);
	
	UFUNCTION(NetMulticast, Reliable)
	void MultiPauseMovie();

protected:
	UFUNCTION(BlueprintNativeEvent)
	void PauseMovieImpl();
	
public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void SeekMovie(const FTimespan& InTimespan, bool bMulticast = false);
	
	UFUNCTION(NetMulticast, Reliable)
	void MultiSeekMovie(const FTimespan& InTimespan);

protected:
	UFUNCTION(BlueprintNativeEvent)
	void SeekMovieImpl(const FTimespan& InTimespan);

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void StopMovie(bool bSkip, bool bMulticast = false);

	UFUNCTION(NetMulticast, Reliable)
	void MultiStopMovie(bool bSkip);

protected:
	UFUNCTION(BlueprintNativeEvent)
	void StopMovieImpl(bool bSkip);

public:
	UPROPERTY(BlueprintAssignable)
	FOnMoviePlayStartingDelegate OnMoviePlayStartingDelegate;
	
	UPROPERTY(BlueprintAssignable)
	FOnMoviePlayFinishedDelegate OnMoviePlayFinishedDelegate;
	
	FOnMoviePlayFinishedSingleDelegate OnMoviePlayFinishedSingleDelegate;
	
	//////////////////////////////////////////////////////////////////////////
	/// Network
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
