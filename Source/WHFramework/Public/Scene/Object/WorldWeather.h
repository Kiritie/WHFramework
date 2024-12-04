// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Common/Base/WHObject.h"
#include "Components/ActorComponent.h"
#include "SaveGame/Base/SaveDataAgentInterface.h"
#include "WorldWeather.generated.h"

class ASkyLight;
class ADirectionalLight;

/**
 * �����������
 */
UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew)
class WHFRAMEWORK_API UWorldWeather : public UWHObject, public ISaveDataAgentInterface
{
	GENERATED_BODY()

public:
	UWorldWeather();

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
	UFUNCTION(BlueprintImplementableEvent, BlueprintPure)
	int32 GetWeatherSeed() const;

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void SetWeatherSeed(int32 InSeed) const;

	UFUNCTION(BlueprintImplementableEvent, BlueprintPure)
	TArray<FParameter> GetWeatherParams() const;

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void ResetWeatherParams() const;

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, meta = (AutoCreateRefTerm = "InParams"))
	void SetWeatherParams(const TArray<FParameter>& InParams) const;

protected:
	UPROPERTY(EditAnywhere)
	bool bAutoSave;

public:
	UFUNCTION(BlueprintPure)
	bool IsAutoSave() const { return bAutoSave; }
};
