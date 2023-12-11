// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Event/Handle/EventHandleBase.h"
#include "Parameter/ParameterModuleTypes.h"

#include "EventHandle_SwitchProcedure.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UEventHandle_SwitchProcedure : public UEventHandleBase
{
	GENERATED_BODY()

public:
	UEventHandle_SwitchProcedure();
	
public:
	virtual void OnDespawn_Implementation(bool bRecovery) override;

	virtual void Parse_Implementation(const TArray<FParameter>& InParams) override;

	virtual TArray<FParameter> Pack_Implementation() const override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<class UProcedureBase> ProcedureClass;
};
