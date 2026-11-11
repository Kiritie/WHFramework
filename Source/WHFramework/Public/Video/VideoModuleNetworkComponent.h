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
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
public:
	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerPlayMovieMulticast(FGuid WorldRoute, AMediaPlayerBase* InMediaPlayer, const FName& InName);

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerPauseMovieMulticast(FGuid WorldRoute, AMediaPlayerBase* InMediaPlayer);

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerSeekMovieMulticast(FGuid WorldRoute, AMediaPlayerBase* InMediaPlayer, const FTimespan& InTimespan);

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerStopMovieMulticast(FGuid WorldRoute, AMediaPlayerBase* InMediaPlayer, bool bSkip);
};
