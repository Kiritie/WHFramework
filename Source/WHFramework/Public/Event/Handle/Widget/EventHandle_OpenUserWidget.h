// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Event/Handle/InstancedEventHandleBase.h"
#include "Parameter/ParameterModuleTypes.h"

#include "EventHandle_OpenUserWidget.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UEventHandle_OpenUserWidget : public UInstancedEventHandleBase
{
	GENERATED_BODY()

public:
	UEventHandle_OpenUserWidget();

public:
	virtual void OnDespawn_Implementation(bool bRecovery) override;

	virtual void Parse_Implementation(const TArray<FParameter>& InParams) override;

	virtual TArray<FParameter> Pack_Implementation() override;

public:
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<class UUserWidgetBase> WidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FParameter> WidgetParams;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bInstant;
};
