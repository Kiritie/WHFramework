// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Event/Handle/InstancedEventHandleBase.h"
#include "Parameter/ParameterModuleTypes.h"
#include "WorldPartition/DataLayer/ActorDataLayer.h"

#include "EventHandle_SetWorldWidgetVisible.generated.h"

class UWorldWidgetBase;
class UDataLayerAsset;

/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UEventHandle_SetWorldWidgetVisible : public UInstancedEventHandleBase
{
	GENERATED_BODY()

public:
	UEventHandle_SetWorldWidgetVisible();

public:
	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	virtual void Parse_Implementation(const TArray<FParameter>& InParams) override;

	virtual TArray<FParameter> Pack_Implementation() override;
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UWorldWidgetBase> WidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bVisible;
};
