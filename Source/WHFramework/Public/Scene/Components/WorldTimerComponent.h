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
	float TimeSeconds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default")
	float SecondsOfDay;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Default")
	int32 CurrentDay;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Default")
	int32 CurrentHour;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Default")
	int32 CurrentMinute;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Default")
	int32 CurrentSeconds;
		
protected:
	virtual void BeginPlay() override;

public:
	UFUNCTION(BlueprintCallable)
	void InitializeTimer(float InSecondsOfDay, float InTimeSeconds = -1.f);

	UFUNCTION(BlueprintCallable)
	void UpdateTimer(float DeltaSeconds = 0.f);

	UFUNCTION(BlueprintCallable)
	void SetCurrentTime(float InTimeSeconds, bool bUpdateTimer = true);

protected:
	UFUNCTION(BlueprintImplementableEvent)
	void UpdateLight(float InLightDirection);
	
public:
	UFUNCTION(BlueprintPure)
	float GetSunriseTime() const;

	UFUNCTION(BlueprintPure)
	float GetNoonTime() const;

	UFUNCTION(BlueprintPure)
	float GetSunsetTime() const;

	UFUNCTION(BlueprintPure)
	float GetSecondsOfDay() const { return SecondsOfDay; }

	UFUNCTION(BlueprintPure)
	float GetTimeSeconds() const { return TimeSeconds; }

	UFUNCTION(BlueprintPure)
	int32 GetCurrentDay() const { return CurrentDay; }

	UFUNCTION(BlueprintPure)
	int32 GetCurrentHour() const { return CurrentHour; }

	UFUNCTION(BlueprintPure)
	int32 GetCurrentMinute() const { return CurrentMinute; }

	UFUNCTION(BlueprintPure)
	int32 GetCurrentSeconds() const { return CurrentSeconds; }
};
