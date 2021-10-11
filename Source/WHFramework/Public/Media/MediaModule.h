// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "MediaModuleTypes.h"
#include "Base/ModuleBase.h"

#include "MediaModule.generated.h"

UCLASS()
class WHFRAMEWORK_API AMediaModule : public AModuleBase
{
	GENERATED_BODY()
	
public:	
	// ParamSets default values for this actor's properties
	AMediaModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate_Implementation() override;

	virtual void OnDestroy_Implementation() override;
#endif

	virtual void OnInitialize_Implementation() override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnPause_Implementation() override;

	virtual void OnUnPause_Implementation() override;

	//////////////////////////////////////////////////////////////////////////
	/// MediaPlayer
protected:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Replicated, Category = "MediaPlayer")
	TArray<class AMediaPlayerBase*> MediaPlayers;

public:
	UFUNCTION(BlueprintCallable)
	void AddMediaPlayerToList(class AMediaPlayerBase* InMediaPlayer);

	UFUNCTION(BlueprintCallable)
	void RemoveMediaPlayerFromList(class AMediaPlayerBase* InMediaPlayer);

	UFUNCTION(BlueprintCallable)
	void RemoveMediaPlayerFromListByName(const FName InMediaPlayerName);

	UFUNCTION(BlueprintPure)
	class AMediaPlayerBase* GetMediaPlayerByName(const FName InMediaPlayerName) const;
	
	//////////////////////////////////////////////////////////////////////////
	/// Movie
public:
	UFUNCTION(BlueprintCallable)
	void PlayMediaPlayerMovie(const FName InMediaPlayerName, const FName InMovieName, bool bMulticast = false);

	UFUNCTION(BlueprintCallable)
	void PlayMovieWithDelegate(const FName InMediaPlayerName, const FName InMovieName, const FOnMoviePlayFinishedSingleDelegate& InOnMoviePlayFinishedDelegate, bool bMulticast = false);
	
	UFUNCTION(BlueprintCallable)
	void StopMediaPlayerMovie(const FName InMediaPlayerName, bool bSkip, bool bMulticast = false);

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
