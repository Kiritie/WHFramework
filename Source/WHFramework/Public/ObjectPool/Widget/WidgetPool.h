// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"
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
	virtual UObject* SpawnImpl() override;

	virtual void DespawnImpl(UObject* InObject) override;
};
