// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Event/Handle/InstancedEventHandleBase.h"
#include "Parameter/ParameterModuleTypes.h"

#include "EventHandle_SetDataLayerOwnerPlayer.generated.h"

class UDataLayerAsset;

/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UEventHandle_SetDataLayerOwnerPlayer : public UInstancedEventHandleBase
{
	GENERATED_BODY()

public:
	UEventHandle_SetDataLayerOwnerPlayer();

public:
	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	virtual void Parse_Implementation(const TArray<FParameter>& InParams) override;

	virtual TArray<FParameter> Pack_Implementation() override;
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	UDataLayerAsset* DataLayer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 PlayerIndex;
};
