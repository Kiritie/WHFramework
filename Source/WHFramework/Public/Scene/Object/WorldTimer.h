// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Common/Base/WHObject.h"
#include "WorldTimer.generated.h"

class ASkyLight;
class ADirectionalLight;

/**
 * ʱ��������
 */
UCLASS(Blueprintable, DefaultToInstanced)
class WHFRAMEWORK_API UWorldTimer : public UWHObject
{
	GENERATED_BODY()

public:
	UWorldTimer();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldTimer")
	ASkyLight* SkyLight;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldTimer")
	ADirectionalLight* SunLight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldTimer")
	float TimeSeconds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldTimer")
	float SecondsOfDay;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "WorldTimer")
	int32 CurrentDay;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "WorldTimer")
	int32 CurrentHour;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "WorldTimer")
	int32 CurrentMinute;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "WorldTimer")
	int32 CurrentSeconds;

	//////////////////////////////////////////////////////////////////////////
	/// Object
public:
	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;
		
	virtual void OnDespawn_Implementation(bool bRecovery) override;

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
