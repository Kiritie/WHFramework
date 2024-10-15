// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Event/Handle/InstancedEventHandleBase.h"
#include "Parameter/ParameterModuleTypes.h"

#include "EventHandle_AsyncLoadLevel.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UEventHandle_AsyncLoadLevel : public UInstancedEventHandleBase
{
	GENERATED_BODY()

public:
	UEventHandle_AsyncLoadLevel();
	
public:
	virtual void OnDespawn_Implementation(bool bRecovery) override;

	virtual void Parse_Implementation(const TArray<FParameter>& InParams) override;

	virtual TArray<FParameter> Pack_Implementation() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName LevelPath;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float FinishDelayTime;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bCreateLoadingWidget;
};
