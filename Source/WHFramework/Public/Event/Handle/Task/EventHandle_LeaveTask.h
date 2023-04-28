// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"
#include "Event/Handle/EventHandleBase.h"
#include "EventHandle_LeaveTask.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UEventHandle_LeaveTask : public UEventHandleBase
{
	GENERATED_BODY()

public:
	UEventHandle_LeaveTask();

public:
	UPROPERTY(BlueprintReadOnly)
	class UTaskBase* Task;
	
public:
	virtual void OnDespawn_Implementation(bool bRecovery) override;

	virtual void Fill_Implementation(const TArray<FParameter>& InParams) override;
};
