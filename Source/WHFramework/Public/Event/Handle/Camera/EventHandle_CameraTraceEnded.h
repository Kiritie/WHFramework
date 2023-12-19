// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Event/Handle/EventHandleBase.h"

#include "EventHandle_CameraTraceEnded.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UEventHandle_CameraTraceEnded : public UEventHandleBase
{
	GENERATED_BODY()

public:
	UEventHandle_CameraTraceEnded();
	
public:
	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	virtual void Parse_Implementation(const TArray<FParameter>& InParams) override;

public:
	UPROPERTY(BlueprintReadOnly)
	AActor* TraceTarget;
};
