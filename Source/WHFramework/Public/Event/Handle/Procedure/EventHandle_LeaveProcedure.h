// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Event/Handle/EventHandleBase.h"
#include "EventHandle_LeaveProcedure.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UEventHandle_LeaveProcedure : public UEventHandleBase
{
	GENERATED_BODY()

public:
	UEventHandle_LeaveProcedure();
	
public:
	virtual void OnDespawn_Implementation(bool bRecovery) override;

	virtual void Parse_Implementation(const TArray<FParameter>& InParams) override;

public:
	UPROPERTY(BlueprintReadOnly)
	class UProcedureBase* Procedure;
};
