// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Event/Handle/InstancedEventHandleBase.h"
#include "Parameter/ParameterModuleTypes.h"

#include "EventHandle_AsyncUnloadLevel.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UEventHandle_AsyncUnloadLevel : public UInstancedEventHandleBase
{
	GENERATED_BODY()

public:
	UEventHandle_AsyncUnloadLevel();
	
public:
	virtual void OnDespawn_Implementation(bool bRecovery) override;

	virtual void Parse_Implementation(const TArray<FParameter>& InParams) override;

	virtual TArray<FParameter> Pack_Implementation() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UWorld> LevelObjectPtr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditConditionHides, EditCondition = "!LevelObjectPtr.IsValid()"))
	FName LevelPath;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float FinishDelayTime;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bCreateLoadingWidget;
};
