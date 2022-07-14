// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Global/Base/WHActor.h"

#include "Media/MediaModuleTypes.h"
#include "Scene/Actor/SceneActorInterface.h"

#include "MediaPlayerBase.generated.h"

/**
 * 
 */
UCLASS(Blueprintable)
class WHFRAMEWORK_API AMediaPlayerBase : public AWHActor, public ISceneActorInterface
{
	GENERATED_BODY()
	
public:
	AMediaPlayerBase();

protected:
	virtual void BeginPlay() override;
	
public:
	virtual void Tick(float DeltaSeconds) override;

	//////////////////////////////////////////////////////////////////////////
	/// Actor
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Actor")
	FGuid ActorID;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Actor")
	TScriptInterface<ISceneContainerInterface> Container;
	
public:
	virtual FGuid GetActorID_Implementation() const override { return ActorID; }

	virtual void SetActorID_Implementation(FGuid InActorID) override { ActorID = InActorID; }

	virtual TScriptInterface<ISceneContainerInterface> GetContainer_Implementation() const override { return Container; }

	virtual void SetContainer_Implementation(const TScriptInterface<ISceneContainerInterface>& InContainer) override { Container = InContainer; }

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
	void PlayMovieWithDelegate(const FName InMovieName, const FOnMoviePlayFinishedSingleDelegate& InOnMoviePlayFinishedDelegate, bool bMulticast = false);
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
