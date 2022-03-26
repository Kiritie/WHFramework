// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/ActorComponent.h"
#include "WorldTimerComponent.generated.h"

class ASkyLight;
class ADirectionalLight;

/**
 * ʱ��������
 */
UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent))
class WHFRAMEWORK_API UWorldTimerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWorldTimerComponent();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default")
	ASkyLight* SkyLight;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default")
	ADirectionalLight* SunLight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default")
	float SecondsOfDay;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default")
	float SunriseTime;
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default")
	float SunsetTime;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Default")
	int TimeSeconds;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Default")
	int CurrentDay;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Default")
	int CurrentHour;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Default")
	int CurrentMinute;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Default")
	int CurrentSeconds;
		
protected:
	virtual void BeginPlay() override;

public:
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent)
	void UpdateTimer();
	
	UFUNCTION(BlueprintCallable)
	void SetTimeSeconds(int InTimeSeconds, bool bUpdateTimer = true);

	UFUNCTION(BlueprintPure)
	float GetSecondsOfDay() const { return SecondsOfDay; }

	UFUNCTION(BlueprintPure)
	float GetSunriseTime() const { return SunriseTime; }

	UFUNCTION(BlueprintPure)
	float GetSunsetTime() const { return SunsetTime; }

	UFUNCTION(BlueprintPure)
	int GetTimeSeconds() const { return TimeSeconds; }

	UFUNCTION(BlueprintPure)
	int GetCurrentDay() const { return CurrentDay; }

	UFUNCTION(BlueprintPure)
	int GetCurrentHour() const { return CurrentHour; }

	UFUNCTION(BlueprintPure)
	int GetCurrentMinute() const { return CurrentMinute; }

	UFUNCTION(BlueprintPure)
	int GetCurrentSeconds() const { return CurrentSeconds; }
};