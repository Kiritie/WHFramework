// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Event/Handle/EventHandleBase.h"
#include "EventHandle_ProcedureStateChanged.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UEventHandle_ProcedureStateChanged : public UEventHandleBase
{
	GENERATED_BODY()

public:
	UEventHandle_ProcedureStateChanged();
	
public:
	virtual void OnDespawn_Implementation(bool bRecovery) override;

	virtual void Parse_Implementation(const TArray<FParameter>& InParams) override;

public:
	UPROPERTY(BlueprintReadOnly)
	class UProcedureBase* Procedure;
};
