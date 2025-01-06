// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Event/Handle/InstancedEventHandleBase.h"
#include "Parameter/ParameterModuleTypes.h"

#include "EventHandle_StopLevelSequence.generated.h"

class ALevelSequenceActor;

/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UEventHandle_StopLevelSequence : public UInstancedEventHandleBase
{
	GENERATED_BODY()

public:
	UEventHandle_StopLevelSequence();

public:
	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	virtual void Parse_Implementation(const TArray<FParameter>& InParams) override;

	virtual TArray<FParameter> Pack_Implementation() override;
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<ALevelSequenceActor> LevelSequence;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Delay;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bKeepState;
};
