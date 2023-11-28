// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Common/Base/WHObject.h"
#include "Components/ActorComponent.h"
#include "WorldWeather.generated.h"

class ASkyLight;
class ADirectionalLight;

/**
 * �����������
 */
UCLASS(Blueprintable, DefaultToInstanced)
class WHFRAMEWORK_API UWorldWeather : public UWHObject
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
	void UpdateWeather(float DeltaSeconds = 0.f);
};
