// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ObjectPool/ObjectPool.h"

#include "UObject/NoExportTypes.h"
#include "WidgetPool.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWidgetPool : public UObjectPool
{
	GENERATED_BODY()

public:
	UWidgetPool();

public:
	virtual UObject* OnSpawn(UObject* InObject) override;

	virtual void OnDespawn(UObject* InObject, bool bRecovery) override;
};
