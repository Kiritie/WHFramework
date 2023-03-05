// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"
#include "Event/Handle/EventHandleBase.h"
#include "Parameter/ParameterModuleTypes.h"

#include "EventHandle_EnterProcedure.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UEventHandle_EnterProcedure : public UEventHandleBase
{
	GENERATED_BODY()

public:
	UEventHandle_EnterProcedure();

public:
	UPROPERTY(BlueprintReadOnly)
	class UProcedureBase* Procedure;
	
public:
	virtual void OnDespawn_Implementation(bool bRecovery) override;

	virtual void Fill_Implementation(const TArray<FParameter>& InParams) override;
};
