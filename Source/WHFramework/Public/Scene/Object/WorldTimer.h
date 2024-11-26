// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Common/Base/WHObject.h"
#include "SaveGame/Base/SaveDataAgentInterface.h"
#include "Scene/SceneModuleTypes.h"
#include "WorldTimer.generated.h"

class ASkyLight;
class ADirectionalLight;

/**
 * ʱ��������
 */
UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew)
class WHFRAMEWORK_API UWorldTimer : public UWHObject, public ISaveDataAgentInterface
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

protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual FSaveData* ToData() override;

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void ResetTimerParams() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void ResetDateTime(float InNewTime = -1.f) const;

	UFUNCTION(BlueprintImplementableEvent, BlueprintPure)
	FDateTime GetDateTime() const;

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void SetDateTime(FDateTime InDataTime) const;

	UFUNCTION(BlueprintImplementableEvent, BlueprintPure)
	float GetCurrentTime() const;

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void SetCurrentTime(float InTime) const;

	UFUNCTION(BlueprintImplementableEvent, BlueprintPure)
	float GetDayLength() const;

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void SetDayLength(float InLength) const;

	UFUNCTION(BlueprintImplementableEvent, BlueprintPure)
	float GetNightLength() const;

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void SetNightLength(float InLength) const;

	UFUNCTION(BlueprintImplementableEvent, BlueprintPure)
	void GetTimeRange(float& OutMinTime, float& OutMaxTime) const;

	UFUNCTION(BlueprintImplementableEvent, BlueprintPure)
	float GetTimePercent() const;

	UFUNCTION(BlueprintImplementableEvent, BlueprintPure)
	float GetSunriseTime() const;

	UFUNCTION(BlueprintImplementableEvent, BlueprintPure)
	float GetNoonTime() const;

	UFUNCTION(BlueprintImplementableEvent, BlueprintPure)
	float GetSunsetTime() const;

	UFUNCTION(BlueprintImplementableEvent, BlueprintPure)
	EWorldTimePhase GetTimePhase() const;

protected:
	UPROPERTY(EditAnywhere)
	bool bAutoSave;

public:
	UFUNCTION(BlueprintPure)
	bool IsAutoSave() const { return bAutoSave; }
};
