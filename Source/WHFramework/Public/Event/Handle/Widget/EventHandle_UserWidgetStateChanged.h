// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFrameworkSlateTypes.h"
#include "Event/Handle/EventHandleBase.h"
#include "Parameter/ParameterModuleTypes.h"

#include "EventHandle_UserWidgetStateChanged.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UEventHandle_UserWidgetStateChanged : public UEventHandleBase
{
	GENERATED_BODY()

public:
	UEventHandle_UserWidgetStateChanged();

public:
	virtual void OnDespawn_Implementation(bool bRecovery) override;

	virtual void Parse_Implementation(const TArray<FParameter>& InParams) override;

public:
	UPROPERTY(BlueprintReadOnly)
	class UUserWidgetBase* UserWidget;

	UPROPERTY(BlueprintReadOnly)
	EScreenWidgetState WidgetState;
};
