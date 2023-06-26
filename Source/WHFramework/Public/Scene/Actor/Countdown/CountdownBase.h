// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"
#include "Global/Base/WHActor.h"
#include "Scene/Actor/SceneActorInterface.h"

#include "CountdownBase.generated.h"

UCLASS()
class WHFRAMEWORK_API ACountdownBase : public AWHActor
{
	GENERATED_BODY()
	
public:	
	ACountdownBase();

public:
	virtual void OnInitialize_Implementation() override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

public:
	UPROPERTY(BlueprintReadWrite)
	float CountdownTime;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ServerCountdownTime)
	int32 ServerCountdownTime;
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, ReplicatedUsing = OnRep_TotalCountdownTime)
	float TotalCountdownTime;

public:
	void ServerSetTotalCountdownTime(float InCountdownTime);

	UFUNCTION()
	void OnRep_TotalCountdownTime();

	UFUNCTION()
	void OnRep_ServerCountdownTime();

	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent)
	void UpdateCountdownDisplayStyle();
};
