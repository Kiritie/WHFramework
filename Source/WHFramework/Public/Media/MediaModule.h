// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "MediaModuleTypes.h"
#include "Main/Base/ModuleBase.h"

#include "MediaModule.generated.h"

class AMediaPlayerBase;
UCLASS()
class WHFRAMEWORK_API AMediaModule : public AModuleBase
{
	GENERATED_BODY()
			
	GENERATED_MODULE(AMediaModule)

public:	
	// ParamSets default values for this actor's properties
	AMediaModule();

	~AMediaModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
#endif

	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnPause_Implementation() override;

	virtual void OnUnPause_Implementation() override;

	virtual void OnTermination_Implementation(EPhase InPhase) override;

	//////////////////////////////////////////////////////////////////////////
	/// MediaPlayer
protected:
	UPROPERTY(EditAnywhere, Replicated, Category = "MediaPlayer")
	TArray<AMediaPlayerBase*> MediaPlayers;

public:
	UFUNCTION(BlueprintCallable)
	void AddMediaPlayerToList(AMediaPlayerBase* InMediaPlayer);

	UFUNCTION(BlueprintCallable)
	void RemoveMediaPlayerFromList(AMediaPlayerBase* InMediaPlayer);

	UFUNCTION(BlueprintCallable)
	void RemoveMediaPlayerFromListByName(const FName InName);

	UFUNCTION(BlueprintPure)
	AMediaPlayerBase* GetMediaPlayerByName(const FName InName) const;
	
	//////////////////////////////////////////////////////////////////////////
	/// Movie
public:
	UFUNCTION(BlueprintCallable)
	void PlayMediaPlayerMovie(const FName InName, const FName InMovieName, bool bMulticast = false);

	UFUNCTION(BlueprintCallable)
	void PlayMovieWithDelegate(const FName InName, const FName InMovieName, const FOnMoviePlayFinishedSingleDelegate& InOnPlayFinished, bool bMulticast = false);
	
	UFUNCTION(BlueprintCallable)
	void StopMediaPlayerMovie(const FName InName, bool bSkip, bool bMulticast = false);

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
