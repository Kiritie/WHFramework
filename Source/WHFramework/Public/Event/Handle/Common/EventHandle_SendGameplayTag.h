// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Common/CommonTypes.h"
#include "Event/Handle/InstancedEventHandleBase.h"
#include "Parameter/ParameterModuleTypes.h"

#include "EventHandle_SendGameplayTag.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UEventHandle_SendGameplayTag : public UInstancedEventHandleBase
{
	GENERATED_BODY()

public:
	UEventHandle_SendGameplayTag();

public:
	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	virtual void Parse_Implementation(const TArray<FParameter>& InParams) override;

	virtual TArray<FParameter> Pack_Implementation() override;
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag Tag;
};
