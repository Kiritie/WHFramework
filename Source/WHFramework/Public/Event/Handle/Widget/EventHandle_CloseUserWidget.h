// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Event/Handle/InstancedEventHandleBase.h"
#include "EventHandle_CloseUserWidget.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UEventHandle_CloseUserWidget : public UInstancedEventHandleBase
{
	GENERATED_BODY()

public:
	UEventHandle_CloseUserWidget();

public:
	virtual void OnDespawn_Implementation(bool bRecovery) override;

	virtual void Parse_Implementation(const TArray<FParameter>& InParams) override;

	virtual TArray<FParameter> Pack_Implementation() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<class UUserWidgetBase> WidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bInstant;
};
