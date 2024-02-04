// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Main/Base/ModuleNetworkComponentBase.h"

#include "VideoModuleNetworkComponent.generated.h"

class AMediaPlayerBase;
/**
* 
*/
UCLASS()
class WHFRAMEWORK_API UVideoModuleNetworkComponent : public UModuleNetworkComponentBase
{
	GENERATED_BODY()

public:
	UVideoModuleNetworkComponent();
	
public:
	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerPlayMovieMulticast(AMediaPlayerBase* InMediaPlayer, const FName& InName);

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerPauseMovieMulticast(AMediaPlayerBase* InMediaPlayer);

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerSeekMovieMulticast(AMediaPlayerBase* InMediaPlayer, const FTimespan& InTimespan);

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerStopMovieMulticast(AMediaPlayerBase* InMediaPlayer, bool bSkip);
};
