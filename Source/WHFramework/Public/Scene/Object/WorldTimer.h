// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Common/Base/WHObject.h"
#include "WorldTimer.generated.h"

class ASkyLight;
class ADirectionalLight;

/**
 * ʱ��������
 */
UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew)
class WHFRAMEWORK_API UWorldTimer : public UWHObject
{
	GENERATED_BODY()

public:
	UWorldTimer();

	//////////////////////////////////////////////////////////////////////////
	/// Object
public:
	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;
		
	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	UFUNCTION(BlueprintImplementableEvent)
	void OnInitialize();

	UFUNCTION(BlueprintImplementableEvent)
	void OnPreparatory();

	UFUNCTION(BlueprintImplementableEvent)
	void OnRefresh(float DeltaSeconds);

	UFUNCTION(BlueprintImplementableEvent)
	void OnPause();

	UFUNCTION(BlueprintImplementableEvent)
	void OnUnPause();

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void ResetDateTime(float InNewTime = -1.f) const;

	UFUNCTION(BlueprintImplementableEvent, BlueprintPure)
	FDateTime GetDateTime() const;

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void SetDateTime(FDateTime InDataTime) const;

	UFUNCTION(BlueprintImplementableEvent, BlueprintPure)
	float GetTimeOfDay() const;

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void SetTimeOfDay(float InTime) const;

	UFUNCTION(BlueprintImplementableEvent, BlueprintPure)
	float GetDayLength() const;

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void SetDayLength(float InLength) const;

	UFUNCTION(BlueprintImplementableEvent, BlueprintPure)
	float GetNightLength() const;

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void SetNightLength(float InLength) const;

	UFUNCTION(BlueprintImplementableEvent, BlueprintPure)
	float GetSunriseTime() const;

	UFUNCTION(BlueprintImplementableEvent, BlueprintPure)
	float GetNoonTime() const;

	UFUNCTION(BlueprintImplementableEvent, BlueprintPure)
	float GetSunsetTime() const;
};
