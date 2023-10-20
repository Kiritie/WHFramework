// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Common/Base/WHActor.h"

#include "Video/VideoModuleTypes.h"

#include "MediaPlayerBase.generated.h"

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
	/// Name
protected:
	UPROPERTY(EditAnywhere, Category = "Default")
	FName Name;
	
public:
	UFUNCTION(BlueprintPure)
	virtual FName GetPlayerName() const { return Name; }
	
	//////////////////////////////////////////////////////////////////////////
	/// Movie
public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void PlayMovie(const FName InMovieName, bool bMulticast = false);
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void PlayMovieWithDelegate(const FName InMovieName, const FOnMoviePlayFinishedSingleDelegate& InOnPlayFinished, bool bMulticast = false);
	UFUNCTION(NetMulticast, Reliable)
	void MultiPlayMovie(const FName InMovieName);
	UFUNCTION(BlueprintNativeEvent)
	void PlayMovieImpl(const FName InMovieName);
	
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void StopMovie(bool bSkip, bool bMulticast = false);
	UFUNCTION(NetMulticast, Reliable)
	void MultiStopMovie(bool bSkip);
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
