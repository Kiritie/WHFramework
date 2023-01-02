// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"

#include "CountdownTypes.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCountdownFinishedDelegate);

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCountdownHelper
{
	GENERATED_BODY()
	
public:
	void StartCountdown(const class UObject* WorldContext);
	void StopCountdown(const class UObject* WorldContext);
	void OnCountdownFinished();

	bool IsCountdown() const;
	
public:
	UPROPERTY(BlueprintReadOnly, BlueprintAssignable)
	FOnCountdownFinishedDelegate OnCountdownFinishedDelegate;

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Countdown")
	TSubclassOf<class ACountdownBase> CountdownClass;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Countdown")
	float CountdownTime;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Countdown")
	class ATargetPoint* CountdownSpawnPosition;
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category="Countdown")
	class ACountdownBase* Countdown;

protected:
	FTimerHandle CountdownTimer;
};
